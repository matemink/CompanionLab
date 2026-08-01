#pragma once

#include "onboard_autonomy/domain/CameraCalibration.hpp"
#include "onboard_autonomy/domain/TargetObservation.hpp"

namespace onboard_autonomy::adapters::vision {

[[nodiscard]] domain::ImagePoint undistort_image_point(
    const domain::ImagePoint& distorted,
    const domain::CameraCalibration& calibration
);

}  // namespace onboard_autonomy::adapters::vision
