#pragma once

#include "onboard_autonomy/domain/CameraCalibration.hpp"

#include <filesystem>
#include <istream>

namespace onboard_autonomy::adapters::vision {

class CameraCalibrationLoader {
public:
    [[nodiscard]] static domain::CameraCalibration from_file(
        const std::filesystem::path& path
    );

    [[nodiscard]] static domain::CameraCalibration from_stream(
        std::istream& input
    );
};

}  // namespace onboard_autonomy::adapters::vision
