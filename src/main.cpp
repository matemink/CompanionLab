#include "onboard_autonomy/adapters/ardupilot/BoardTypeCatalog.hpp"
#include "onboard_autonomy/adapters/camera/RpicamCameraSource.hpp"
#include "onboard_autonomy/adapters/preview/HttpCameraPreviewServer.hpp"
#include "onboard_autonomy/adapters/transport/TransportFactory.hpp"
#include "onboard_autonomy/adapters/vision/AprilTagTargetDetector.hpp"
#include "onboard_autonomy/adapters/vision/CameraCalibrationLoader.hpp"
#include "onboard_autonomy/application/CompanionApplication.hpp"
#include "onboard_autonomy/presentation/console/ConsoleInput.hpp"
#include "onboard_autonomy/presentation/console/ConsoleView.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::atomic_bool keep_running{true};

void handle_signal(int) {
    keep_running = false;
}

struct Options {
    std::string udp_bind{"0.0.0.0"};
    std::uint16_t udp_port{14550};
    std::string serial_device;
    std::uint32_t baud_rate{115200};
    std::uint32_t snapshot_interval_ms{1000};
    bool camera_enabled{false};
    bool apriltag_enabled{false};
    std::string camera_calibration_file;
    std::optional<double> apriltag_tag_size_m;
    bool camera_preview_enabled{false};
    std::uint16_t camera_preview_port{8080};
    std::uint32_t camera_width{640};
    std::uint32_t camera_height{480};
    std::uint32_t camera_fps{30};
    std::string board_types_file;
    bool json_output{false};
    std::optional<onboard_autonomy::application::ScenarioId>
        startup_scenario;
    bool exit_after_scenario{false};
    bool interactive{false};
    bool show_help{false};
};

class ConsoleSession {
public:
    explicit ConsoleSession(const bool active)
        : active_(active) {
        if (active_) {
            std::cout << "\x1b[2J\x1b[H\x1b[?25l" << std::flush;
        }
    }

    ~ConsoleSession() {
        if (active_) {
            std::cout << "\x1b[?25h\x1b[0m\n" << std::flush;
        }
    }

    ConsoleSession(const ConsoleSession&) = delete;
    ConsoleSession& operator=(const ConsoleSession&) = delete;

private:
    bool active_;
};

template <typename T>
T parse_number(const std::string_view text, const std::string_view name) {
    T value{};
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value
    );
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument(
            "Invalid numeric value for " + std::string(name)
        );
    }
    return value;
}

Options parse_options(const int argc, char** argv) {
    Options options;

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto require_value = [&]() -> std::string_view {
            if (index + 1 >= argc) {
                throw std::invalid_argument(
                    "Missing value after " + std::string(argument)
                );
            }
            ++index;
            return argv[index];
        };

        if (argument == "--udp-bind") {
            options.udp_bind = require_value();
        } else if (argument == "--udp-port") {
            options.udp_port = parse_number<std::uint16_t>(
                require_value(),
                argument
            );
        } else if (argument == "--serial") {
            options.serial_device = require_value();
        } else if (argument == "--baud") {
            options.baud_rate = parse_number<std::uint32_t>(
                require_value(),
                argument
            );
        } else if (argument == "--snapshot-ms") {
            options.snapshot_interval_ms = parse_number<std::uint32_t>(
                require_value(),
                argument
            );
        } else if (argument == "--camera") {
            options.camera_enabled = true;
        } else if (argument == "--apriltag") {
            options.apriltag_enabled = true;
        } else if (argument == "--camera-calibration") {
            options.camera_calibration_file = require_value();
        } else if (argument == "--apriltag-size-mm") {
            options.apriltag_tag_size_m =
                parse_number<double>(require_value(), argument) /
                1000.0;
        } else if (argument == "--camera-preview") {
            options.camera_preview_enabled = true;
        } else if (argument == "--camera-preview-port") {
            options.camera_preview_port =
                parse_number<std::uint16_t>(
                    require_value(),
                    argument
                );
        } else if (argument == "--camera-width") {
            options.camera_width = parse_number<std::uint32_t>(
                require_value(),
                argument
            );
        } else if (argument == "--camera-height") {
            options.camera_height = parse_number<std::uint32_t>(
                require_value(),
                argument
            );
        } else if (argument == "--camera-fps") {
            options.camera_fps = parse_number<std::uint32_t>(
                require_value(),
                argument
            );
        } else if (argument == "--board-types") {
            options.board_types_file = require_value();
        } else if (argument == "--json") {
            options.json_output = true;
        } else if (argument == "--demo-flight") {
            if (options.startup_scenario.has_value()) {
                throw std::invalid_argument(
                    "only one startup scenario may be selected"
                );
            }
            options.startup_scenario =
                onboard_autonomy::application::ScenarioId::hover_check;
        } else if (argument == "--scenario") {
            if (options.startup_scenario.has_value()) {
                throw std::invalid_argument(
                    "only one startup scenario may be selected"
                );
            }
            const auto value = parse_number<std::uint8_t>(
                require_value(),
                argument
            );
            if (value < 1 || value > 5) {
                throw std::invalid_argument(
                    "--scenario must be between 1 and 5"
                );
            }
            options.startup_scenario =
                static_cast<
                    onboard_autonomy::application::ScenarioId
                >(value);
        } else if (argument == "--exit-after-scenario") {
            options.exit_after_scenario = true;
        } else if (argument == "--interactive") {
            options.interactive = true;
        } else if (argument == "--help" || argument == "-h") {
            options.show_help = true;
        } else {
            throw std::invalid_argument(
                "Unknown argument: " + std::string(argument)
            );
        }
    }

    return options;
}

std::optional<
    onboard_autonomy::adapters::ardupilot::BoardTypeCatalog
> load_board_type_catalog(
    const Options& options,
    const std::filesystem::path& executable
) {
    std::vector<std::filesystem::path> candidates;
    if (!options.board_types_file.empty()) {
        candidates.emplace_back(options.board_types_file);
    } else {
        candidates.push_back(
            (
                executable.parent_path() /
                ".." /
                "share" /
                "onboard_autonomy" /
                "ardupilot-board-types.txt"
            ).lexically_normal()
        );
        candidates.emplace_back(
            "third_party/ardupilot/board_types.txt"
        );
    }

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(candidate, error)) {
            continue;
        }
        return onboard_autonomy::adapters::ardupilot::
            BoardTypeCatalog::from_file(candidate);
    }

    if (!options.board_types_file.empty()) {
        throw std::runtime_error(
            "board type table not found: " +
            options.board_types_file
        );
    }
    return std::nullopt;
}

std::filesystem::path find_camera_preview_page(
    const std::filesystem::path& executable
) {
    const std::vector<std::filesystem::path> candidates{
        (
            executable.parent_path() /
            ".." /
            "share" /
            "onboard_autonomy" /
            "camera-preview.html"
        ).lexically_normal(),
        "assets/camera-preview/index.html",
    };

    for (const auto& candidate : candidates) {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error)) {
            return candidate;
        }
    }

    throw std::runtime_error(
        "camera preview page was not found"
    );
}

void print_help() {
    std::cout
        << "OnboardAutonomy companion service\n\n"
        << "SITL UDP mode:\n"
        << "  onboard_autonomy [--udp-bind 0.0.0.0]"
        << " [--udp-port 14550]\n\n"
        << "Pixhawk serial mode on Linux:\n"
        << "  onboard_autonomy --serial /dev/ttyACM0"
        << " [--baud 115200]\n\n"
        << "Options:\n"
        << "  --snapshot-ms N   Refresh/output interval, default 1000\n"
        << "  --camera          Receive Camera Module frames via rpicam\n"
        << "  --camera-width N  YUV420 width, default 640\n"
        << "  --camera-height N YUV420 height, default 480\n"
        << "  --camera-fps N    Capture rate, default 30\n"
        << "  --apriltag        Detect tagStandard41h12 targets\n"
        << "  --camera-calibration FILE"
        << "  Verified camera calibration JSON\n"
        << "  --apriltag-size-mm N"
        << "  Physical span between detection corners\n"
        << "  --camera-preview  Serve live grayscale preview over HTTP\n"
        << "  --camera-preview-port N"
        << " HTTP preview port, default 8080\n"
        << "  --board-types FILE Override ArduPilot board table path\n"
        << "  --json            Print machine-readable JSON snapshots\n"
        << "  --demo-flight     Run the guarded 5 m SITL flight demo\n"
        << "  --scenario N      Run SITL scenario 1..5\n"
        << "  --exit-after-scenario"
        << "  Exit when startup scenario completes\n"
        << "  --interactive     Enable SITL keyboard scenario triggers\n"
        << "  --help             Show this help\n";
}

}  // namespace

int main(const int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        if (options.show_help) {
            print_help();
            return 0;
        }

        std::signal(SIGINT, handle_signal);
        std::signal(SIGTERM, handle_signal);

        auto board_type_catalog = options.json_output
            ? std::optional<
                  onboard_autonomy::adapters::ardupilot::BoardTypeCatalog
              >{}
            : load_board_type_catalog(options, argv[0]);

        if (options.exit_after_scenario &&
            !options.startup_scenario.has_value()) {
            throw std::invalid_argument(
                "--exit-after-scenario requires --scenario"
            );
        }
        if ((options.apriltag_enabled ||
             options.camera_preview_enabled) &&
            !options.camera_enabled) {
            throw std::invalid_argument(
                "AprilTag detection and camera preview require "
                "--camera"
            );
        }
        const bool has_camera_calibration =
            !options.camera_calibration_file.empty();
        if (has_camera_calibration !=
            options.apriltag_tag_size_m.has_value()) {
            throw std::invalid_argument(
                "camera calibration and AprilTag size must be "
                "provided together"
            );
        }
        if (has_camera_calibration &&
            !options.apriltag_enabled) {
            throw std::invalid_argument(
                "AprilTag pose requires --apriltag"
            );
        }

        if ((options.startup_scenario.has_value() ||
             options.interactive) &&
            !options.serial_device.empty()) {
            throw std::invalid_argument(
                "motion commands are restricted to UDP/SITL; "
                "serial flight controllers are not allowed"
            );
        }

        onboard_autonomy::presentation::console::ConsoleInput console_input{
            options.interactive && !options.json_output
        };
        if (options.interactive && !console_input.active()) {
            throw std::invalid_argument(
                "--interactive requires a live terminal"
            );
        }

        std::unique_ptr<
            onboard_autonomy::application::ports::Transport
        > transport;
        if (options.serial_device.empty()) {
            transport =
                onboard_autonomy::adapters::transport::make_udp_transport(
                options.udp_bind,
                options.udp_port
            );
        } else {
            transport =
                onboard_autonomy::adapters::transport::make_serial_transport(
                options.serial_device,
                options.baud_rate
            );
        }

        std::unique_ptr<
            onboard_autonomy::application::ports::CameraSource
        > camera_source;
        if (options.camera_enabled) {
            camera_source =
                onboard_autonomy::adapters::camera::
                    make_rpicam_camera_source(
                        {
                            .width = options.camera_width,
                            .height = options.camera_height,
                            .frames_per_second =
                                options.camera_fps,
                        }
                    );
        }
        std::unique_ptr<
            onboard_autonomy::application::ports::TargetDetector
        > target_detector;
        if (options.apriltag_enabled) {
            onboard_autonomy::adapters::vision::
                AprilTagDetectorConfig detector_config;
            if (has_camera_calibration) {
                auto calibration =
                    onboard_autonomy::adapters::vision::
                        CameraCalibrationLoader::from_file(
                            options.camera_calibration_file
                        );
                if (calibration.image_width !=
                        options.camera_width ||
                    calibration.image_height !=
                        options.camera_height) {
                    throw std::invalid_argument(
                        "camera runtime resolution does not match "
                        "calibration"
                    );
                }
                detector_config.pose =
                    onboard_autonomy::adapters::vision::
                        AprilTagPoseConfig{
                            .calibration = std::move(calibration),
                            .tag_size_m =
                                *options.apriltag_tag_size_m,
                        };
            }
            target_detector =
                onboard_autonomy::adapters::vision::
                    make_apriltag_target_detector(
                        std::move(detector_config)
                    );
        }
        std::unique_ptr<
            onboard_autonomy::application::ports::CameraPreviewSink
        > camera_preview;
        if (options.camera_preview_enabled) {
            camera_preview =
                onboard_autonomy::adapters::preview::
                    make_http_camera_preview_server(
                        {
                            .bind_address = "0.0.0.0",
                            .port = options.camera_preview_port,
                            .maximum_frames_per_second = 10,
                            .page_file =
                                find_camera_preview_page(argv[0]),
                        }
                    );
        }

        onboard_autonomy::application::CompanionApplication application{
            *transport,
            {
                .scenario_runner =
                    {
                        .enabled =
                            options.startup_scenario.has_value(),
                        .initial_scenario =
                            options.startup_scenario.value_or(
                                onboard_autonomy::application::ScenarioId::
                                    hover_check
                            ),
                    },
                .motion_commands_allowed =
                    options.startup_scenario.has_value() ||
                    console_input.active(),
                .camera_source = camera_source.get(),
                .target_detector = target_detector.get(),
                .camera_preview_sink = camera_preview.get(),
            }
        };
        auto next_snapshot = std::chrono::steady_clock::now();
        bool startup_scenario_failed = false;
        const auto configured_snapshot_interval =
            std::chrono::milliseconds(options.snapshot_interval_ms);
        const auto snapshot_interval = options.json_output
            ? configured_snapshot_interval
            : std::min(
                  configured_snapshot_interval,
                  std::chrono::milliseconds(100)
              );

        std::cerr << "OnboardAutonomy listening on "
                  << transport->description() << '\n';
        if (camera_preview != nullptr) {
            std::cerr << "Camera preview: http://companionpi.local:"
                      << options.camera_preview_port << "/\n";
        }
        ConsoleSession console_session{!options.json_output};

        while (keep_running) {
            const auto now = std::chrono::steady_clock::now();

            while (const auto key = console_input.poll()) {
                if (*key >= '1' && *key <= '5') {
                    const auto scenario_id =
                        static_cast<
                            onboard_autonomy::application::ScenarioId
                        >(*key - '0');
                    static_cast<void>(
                        application.trigger_scenario(
                            scenario_id,
                            now
                        )
                    );
                    next_snapshot = now;
                } else if (*key == 'l' || *key == 'L') {
                    static_cast<void>(
                        application.request_land(now)
                    );
                    next_snapshot = now;
                } else if (*key == 'q' || *key == 'Q') {
                    keep_running = false;
                }
            }
            if (!keep_running) {
                break;
            }

            application.poll(now);

            if (now >= next_snapshot) {
                const auto snapshot = application.snapshot(now);
                if (options.json_output) {
                    std::cout
                        << snapshot.to_json()
                        << std::endl;
                } else {
                    std::cout
                        << "\x1b[H"
                        << onboard_autonomy::presentation::console::render_console(
                               snapshot,
                               transport->description(),
                               true,
                               board_type_catalog.has_value()
                                   ? &*board_type_catalog
                                   : nullptr
                           )
                        << std::flush;
                }
                if (options.exit_after_scenario &&
                    (snapshot.scenario.phase ==
                         onboard_autonomy::application::
                             ScenarioRunnerPhase::completed ||
                     snapshot.scenario.phase ==
                         onboard_autonomy::application::
                             ScenarioRunnerPhase::failed)) {
                    startup_scenario_failed =
                        snapshot.scenario.phase ==
                        onboard_autonomy::application::
                            ScenarioRunnerPhase::failed;
                    keep_running = false;
                }
                next_snapshot = now + snapshot_interval;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        return startup_scenario_failed ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "OnboardAutonomy error: " << error.what() << '\n';
        return 1;
    }
}
