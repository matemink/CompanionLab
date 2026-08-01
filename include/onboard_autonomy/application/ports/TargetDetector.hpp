#pragma once

#include "onboard_autonomy/application/ports/CameraSource.hpp"
#include "onboard_autonomy/domain/TargetObservation.hpp"

#include <string>

namespace onboard_autonomy::application::ports {

class TargetDetector {
public:
    virtual ~TargetDetector() = default;

    [[nodiscard]] virtual domain::TargetDetectionBatch detect(
        const CameraFrame& frame
    ) = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

}  // namespace onboard_autonomy::application::ports
