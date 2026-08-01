#pragma once

#include "onboard_autonomy/application/ports/CameraSource.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace onboard_autonomy::adapters::camera {

struct RpicamCameraConfig {
    std::uint32_t width{640};
    std::uint32_t height{480};
    std::uint32_t frames_per_second{30};
    std::uint32_t camera_index{0};
    std::string command{"rpicam-vid"};
};

[[nodiscard]] std::optional<std::int64_t>
parse_rpicam_frame_wall_clock_ns(std::string_view line);

[[nodiscard]] std::unique_ptr<
    application::ports::CameraSource
> make_rpicam_camera_source(RpicamCameraConfig config = {});

}  // namespace onboard_autonomy::adapters::camera
