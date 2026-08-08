#pragma once

#include "onboard_autonomy/domain/TargetTransform.hpp"

#include <filesystem>
#include <istream>

namespace onboard_autonomy::adapters::vision {

class CameraExtrinsicsLoader {
public:
    [[nodiscard]] static domain::CameraExtrinsics from_file(
        const std::filesystem::path& path
    );

    [[nodiscard]] static domain::CameraExtrinsics from_stream(
        std::istream& input
    );
};

}  // namespace onboard_autonomy::adapters::vision
