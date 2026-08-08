#pragma once

#include "onboard_autonomy/application/ports/CameraSource.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace onboard_autonomy::adapters::camera {

struct GStreamerCameraConfig {
    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint16_t udp_port{5601};
    std::uint32_t jitter_latency_ms{50};
    std::uint32_t frame_timeout_ms{2000};
    std::uint32_t restart_delay_ms{500};
    std::string command{"gst-launch-1.0"};
};

[[nodiscard]] std::vector<std::string>
make_gstreamer_camera_arguments(
    const GStreamerCameraConfig& config
);

[[nodiscard]] std::unique_ptr<
    application::ports::CameraSource
> make_gstreamer_camera_source(
    GStreamerCameraConfig config = {}
);

}  // namespace onboard_autonomy::adapters::camera
