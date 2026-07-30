#include "companionlab/adapters/preview/HttpCameraPreviewServer.hpp"

#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace companionlab::adapters::preview {
namespace {

struct PreviewFrame {
    std::uint64_t sequence{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> luma;
    std::vector<domain::TargetObservation> targets;
};

std::chrono::microseconds checked_frame_interval(
    const std::uint32_t maximum_frames_per_second
) {
    if (maximum_frames_per_second == 0U ||
        maximum_frames_per_second > 60U) {
        throw std::invalid_argument(
            "preview frame rate must be between 1 and 60"
        );
    }
    return std::chrono::microseconds{
        1'000'000 / maximum_frames_per_second,
    };
}

std::string read_page(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "unable to open camera preview page: " +
            path.string()
        );
    }
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{},
    };
}

std::string targets_json(
    const std::vector<domain::TargetObservation>& targets
) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << '[';
    bool first_target = true;
    for (const auto& target : targets) {
        if (!first_target) {
            output << ',';
        }
        first_target = false;
        output << "{\"id\":" << target.id
               << ",\"center\":[" << target.center.x_px << ','
               << target.center.y_px << ']'
               << ",\"corners\":[";
        bool first_corner = true;
        for (const auto& corner : target.corners) {
            if (!first_corner) {
                output << ',';
            }
            first_corner = false;
            output << '[' << corner.x_px << ',' << corner.y_px
                   << ']';
        }
        output << "],\"decision_margin\":"
               << target.decision_margin
               << ",\"corrected_bits\":"
               << target.corrected_bits << '}';
    }
    output << ']';
    return output.str();
}

class HttpCameraPreviewServer final
    : public application::ports::CameraPreviewSink {
public:
    explicit HttpCameraPreviewServer(
        HttpCameraPreviewConfig config
    )
        : config_(std::move(config)),
          page_(read_page(config_.page_file)),
          minimum_frame_interval_(
              checked_frame_interval(
                  config_.maximum_frames_per_second
              )
          ) {
        if (config_.bind_address.empty() ||
            config_.port == 0U) {
            throw std::invalid_argument(
                "invalid HTTP camera preview configuration"
            );
        }

        server_.Get(
            "/",
            [this](
                const httplib::Request&,
                httplib::Response& response
            ) {
                response.set_header("Cache-Control", "no-store");
                response.set_content(
                    page_,
                    "text/html; charset=utf-8"
                );
            }
        );
        server_.Get(
            "/api/frame",
            [this](
                const httplib::Request&,
                httplib::Response& response
            ) {
                PreviewFrame frame;
                {
                    std::scoped_lock lock(frame_mutex_);
                    if (!latest_frame_.has_value()) {
                        response.status = 204;
                        response.set_header(
                            "Cache-Control",
                            "no-store"
                        );
                        return;
                    }
                    frame = *latest_frame_;
                }

                response.set_header("Cache-Control", "no-store");
                response.set_header(
                    "X-Frame-Width",
                    std::to_string(frame.width)
                );
                response.set_header(
                    "X-Frame-Height",
                    std::to_string(frame.height)
                );
                response.set_header(
                    "X-Frame-Sequence",
                    std::to_string(frame.sequence)
                );
                response.set_header(
                    "X-CompanionLab-Targets",
                    targets_json(frame.targets)
                );
                response.set_content(
                    std::string{
                        reinterpret_cast<const char*>(
                            frame.luma.data()
                        ),
                        frame.luma.size(),
                    },
                    "application/octet-stream"
                );
            }
        );

        worker_ = std::jthread([this] {
            const bool listened = server_.listen(
                config_.bind_address,
                static_cast<int>(config_.port)
            );
            if (!listened && !stopping_.load()) {
                failed_.store(true);
            }
        });
    }

    ~HttpCameraPreviewServer() override {
        stopping_.store(true);
        server_.stop();
    }

    void publish(
        const application::ports::CameraFrame& frame,
        const std::span<const domain::TargetObservation> targets
    ) override {
        const auto now = std::chrono::steady_clock::now();
        std::scoped_lock lock(frame_mutex_);
        if (last_published_at_.has_value() &&
            now - *last_published_at_ < minimum_frame_interval_) {
            return;
        }

        const std::uint64_t luma_size =
            static_cast<std::uint64_t>(frame.width) *
            static_cast<std::uint64_t>(frame.height);
        if (frame.width == 0U || frame.height == 0U ||
            luma_size > frame.yuv420.size() ||
            luma_size >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::size_t>::max()
                )) {
            return;
        }

        const auto end = frame.yuv420.begin() +
            static_cast<std::ptrdiff_t>(luma_size);
        latest_frame_ = PreviewFrame{
            .sequence = frame.sequence,
            .width = frame.width,
            .height = frame.height,
            .luma = {frame.yuv420.begin(), end},
            .targets = {targets.begin(), targets.end()},
        };
        last_published_at_ = now;
    }

    [[nodiscard]] std::string description() const override {
        return "http://" + config_.bind_address + ':' +
            std::to_string(config_.port) +
            (failed_.load() ? " (listen failed)" : "");
    }

private:
    HttpCameraPreviewConfig config_;
    std::string page_;
    std::chrono::microseconds minimum_frame_interval_;
    mutable std::mutex frame_mutex_;
    std::optional<PreviewFrame> latest_frame_;
    std::optional<std::chrono::steady_clock::time_point>
        last_published_at_;
    httplib::Server server_;
    std::atomic_bool stopping_{false};
    std::atomic_bool failed_{false};
    std::jthread worker_;
};

}  // namespace

std::unique_ptr<application::ports::CameraPreviewSink>
make_http_camera_preview_server(HttpCameraPreviewConfig config) {
    return std::make_unique<HttpCameraPreviewServer>(
        std::move(config)
    );
}

}  // namespace companionlab::adapters::preview
