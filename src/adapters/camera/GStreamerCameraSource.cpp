#include "onboard_autonomy/adapters/camera/GStreamerCameraSource.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <csignal>
#include <cstring>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace onboard_autonomy::adapters::camera {
namespace {

std::size_t checked_frame_size(
    const GStreamerCameraConfig& config
) {
    if (config.width == 0U || config.height == 0U ||
        config.udp_port == 0U ||
        config.width % 2U != 0U || config.height % 2U != 0U) {
        throw std::invalid_argument(
            "GStreamer camera dimensions and UDP port must be "
            "positive; I420 dimensions must be even"
        );
    }

    const std::uint64_t pixels =
        static_cast<std::uint64_t>(config.width) *
        static_cast<std::uint64_t>(config.height);
    const std::uint64_t bytes = pixels + pixels / 2U;
    if (bytes >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max()
        )) {
        throw std::invalid_argument(
            "GStreamer camera frame size is too large"
        );
    }
    return static_cast<std::size_t>(bytes);
}

#if defined(__linux__)

class GStreamerCameraSource final
    : public application::ports::CameraSource {
public:
    explicit GStreamerCameraSource(GStreamerCameraConfig config)
        : config_(std::move(config)),
          frame_size_(checked_frame_size(config_)) {
        status_.description =
            "GStreamer RTP/H.264 UDP " +
            std::to_string(config_.udp_port);
        worker_ = std::jthread(
            [this](const std::stop_token stop_token) {
                run(stop_token);
            }
        );
    }

    ~GStreamerCameraSource() override {
        worker_.request_stop();
        stop_child();
    }

    [[nodiscard]] std::optional<
        application::ports::CameraFrame
    > take_latest_frame() override {
        std::scoped_lock lock(state_mutex_);
        auto frame = std::move(latest_frame_);
        latest_frame_.reset();
        return frame;
    }

    [[nodiscard]] application::ports::CameraSourceStatus
    status() const override {
        std::scoped_lock lock(state_mutex_);
        return status_;
    }

private:
    enum class ReadResult {
        complete,
        stopped,
        end_of_file,
        failed,
    };

    void run(const std::stop_token stop_token) {
        int video_pipe[2]{-1, -1};
        int error_pipe[2]{-1, -1};
        if (::pipe(video_pipe) != 0 ||
            ::pipe(error_pipe) != 0) {
            set_failure(
                "unable to create GStreamer pipes: " +
                std::string(std::strerror(errno))
            );
            close_pipe(video_pipe);
            close_pipe(error_pipe);
            return;
        }

        auto arguments = make_gstreamer_camera_arguments(config_);
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1U);
        for (auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);

        const pid_t child = ::fork();
        if (child == -1) {
            set_failure(
                "unable to fork GStreamer: " +
                std::string(std::strerror(errno))
            );
            close_pipe(video_pipe);
            close_pipe(error_pipe);
            return;
        }

        if (child == 0) {
            ::close(video_pipe[0]);
            ::close(error_pipe[0]);
            if (::dup2(video_pipe[1], STDOUT_FILENO) == -1 ||
                ::dup2(error_pipe[1], STDERR_FILENO) == -1) {
                _exit(126);
            }
            ::close(video_pipe[1]);
            ::close(error_pipe[1]);
            ::execvp(argv[0], argv.data());
            _exit(127);
        }

        child_pid_.store(child);
        ::close(video_pipe[1]);
        video_pipe[1] = -1;
        ::close(error_pipe[1]);
        error_pipe[1] = -1;

        std::thread error_reader{
            [this, fd = error_pipe[0]] {
                read_lines(
                    fd,
                    [this](const std::string_view line) {
                        if (line.empty()) {
                            return;
                        }
                        std::scoped_lock lock(state_mutex_);
                        last_process_message_ = std::string(line);
                    }
                );
            }
        };

        std::uint64_t sequence = 0U;
        ReadResult read_result = ReadResult::complete;
        while (!stop_token.stop_requested()) {
            application::ports::CameraFrame frame{
                .sequence = ++sequence,
                .width = config_.width,
                .height = config_.height,
                .yuv420 = std::vector<std::uint8_t>(frame_size_),
                .captured_at = std::nullopt,
                .received_at = {},
            };
            read_result = read_exact(
                video_pipe[0],
                frame.yuv420,
                stop_token
            );
            if (read_result != ReadResult::complete) {
                break;
            }
            frame.received_at = std::chrono::system_clock::now();
            publish(std::move(frame));
        }

        if (!stop_token.stop_requested()) {
            ::kill(child, SIGINT);
        }
        int child_status = 0;
        while (::waitpid(child, &child_status, 0) == -1 &&
               errno == EINTR) {
        }
        child_pid_.store(-1);

        ::close(video_pipe[0]);
        error_reader.join();
        ::close(error_pipe[0]);

        if (stop_token.stop_requested()) {
            set_stopped();
            return;
        }
        if (status().phase ==
            application::ports::CameraSourcePhase::failed) {
            return;
        }

        std::string detail;
        {
            std::scoped_lock lock(state_mutex_);
            detail = last_process_message_;
        }
        if (WIFEXITED(child_status)) {
            set_failure(
                "gst-launch-1.0 exited with status " +
                std::to_string(WEXITSTATUS(child_status)) +
                (detail.empty() ? "" : ": " + detail)
            );
        } else if (WIFSIGNALED(child_status)) {
            set_failure(
                "gst-launch-1.0 stopped by signal " +
                std::to_string(WTERMSIG(child_status))
            );
        } else if (read_result == ReadResult::failed) {
            set_failure("failed to read the GStreamer I420 stream");
        } else {
            set_failure("GStreamer I420 stream ended unexpectedly");
        }
    }

    static void close_pipe(int (&descriptors)[2]) {
        for (int& descriptor : descriptors) {
            if (descriptor >= 0) {
                ::close(descriptor);
                descriptor = -1;
            }
        }
    }

    template <typename Consumer>
    static void read_lines(const int fd, Consumer consume) {
        std::string pending;
        std::vector<char> buffer(4096);
        while (true) {
            const ssize_t count =
                ::read(fd, buffer.data(), buffer.size());
            if (count == 0) {
                break;
            }
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            pending.append(
                buffer.data(),
                static_cast<std::size_t>(count)
            );
            std::size_t newline = 0U;
            while ((newline = pending.find('\n')) !=
                   std::string::npos) {
                consume(
                    std::string_view{pending}.substr(0U, newline)
                );
                pending.erase(0U, newline + 1U);
            }
        }
        if (!pending.empty()) {
            consume(pending);
        }
    }

    static ReadResult read_exact(
        const int fd,
        std::vector<std::uint8_t>& destination,
        const std::stop_token stop_token
    ) {
        std::size_t offset = 0U;
        while (offset < destination.size()) {
            if (stop_token.stop_requested()) {
                return ReadResult::stopped;
            }
            pollfd descriptor{
                .fd = fd,
                .events = POLLIN,
                .revents = 0,
            };
            const int ready = ::poll(&descriptor, 1, 100);
            if (ready == 0) {
                continue;
            }
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return ReadResult::failed;
            }
            const ssize_t count = ::read(
                fd,
                destination.data() + offset,
                destination.size() - offset
            );
            if (count == 0) {
                return ReadResult::end_of_file;
            }
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return ReadResult::failed;
            }
            offset += static_cast<std::size_t>(count);
        }
        return ReadResult::complete;
    }

    void publish(application::ports::CameraFrame frame) {
        std::scoped_lock lock(state_mutex_);
        if (latest_frame_.has_value()) {
            ++status_.overwritten_frames;
        }
        latest_frame_ = std::move(frame);
        ++status_.produced_frames;
        status_.phase =
            application::ports::CameraSourcePhase::streaming;
        status_.error.clear();
    }

    void set_failure(std::string error) {
        std::scoped_lock lock(state_mutex_);
        status_.phase =
            application::ports::CameraSourcePhase::failed;
        status_.error = std::move(error);
    }

    void set_stopped() {
        std::scoped_lock lock(state_mutex_);
        status_.phase =
            application::ports::CameraSourcePhase::stopped;
    }

    void stop_child() {
        const pid_t child = child_pid_.load();
        if (child > 0) {
            ::kill(child, SIGINT);
        }
    }

    GStreamerCameraConfig config_;
    std::size_t frame_size_{0U};
    mutable std::mutex state_mutex_;
    application::ports::CameraSourceStatus status_;
    std::optional<application::ports::CameraFrame> latest_frame_;
    std::string last_process_message_;
    std::atomic<pid_t> child_pid_{-1};
    std::jthread worker_;
};

#endif

}  // namespace

std::vector<std::string> make_gstreamer_camera_arguments(
    const GStreamerCameraConfig& config
) {
    static_cast<void>(checked_frame_size(config));
    return {
        config.command,
        "-q",
        "-e",
        "udpsrc",
        "port=" + std::to_string(config.udp_port),
        "caps=application/x-rtp,media=video,clock-rate=90000,"
            "encoding-name=H264,payload=96",
        "!",
        "rtpjitterbuffer",
        "latency=" + std::to_string(config.jitter_latency_ms),
        "drop-on-latency=true",
        "!",
        "rtph264depay",
        "!",
        "h264parse",
        "!",
        "avdec_h264",
        "!",
        "videoconvert",
        "!",
        "video/x-raw,format=I420,width=" +
            std::to_string(config.width) +
            ",height=" + std::to_string(config.height),
        "!",
        "fdsink",
        "fd=1",
        "sync=false",
    };
}

std::unique_ptr<application::ports::CameraSource>
make_gstreamer_camera_source(GStreamerCameraConfig config) {
    static_cast<void>(checked_frame_size(config));
#if defined(__linux__)
    return std::make_unique<GStreamerCameraSource>(
        std::move(config)
    );
#else
    throw std::runtime_error(
        "GStreamer camera source is only available on Linux"
    );
#endif
}

}  // namespace onboard_autonomy::adapters::camera
