#pragma once

#include "companionlab/application/ports/CameraSource.hpp"
#include "companionlab/domain/TargetObservation.hpp"

#include <string>

namespace companionlab::application::ports {

class TargetDetector {
public:
    virtual ~TargetDetector() = default;

    [[nodiscard]] virtual domain::TargetDetectionBatch detect(
        const CameraFrame& frame
    ) = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

}  // namespace companionlab::application::ports
