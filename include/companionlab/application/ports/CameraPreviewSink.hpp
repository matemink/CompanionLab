#pragma once

#include "companionlab/application/ports/CameraSource.hpp"
#include "companionlab/domain/TargetObservation.hpp"

#include <span>
#include <string>

namespace companionlab::application::ports {

class CameraPreviewSink {
public:
    virtual ~CameraPreviewSink() = default;

    virtual void publish(
        const CameraFrame& frame,
        std::span<const domain::TargetObservation> targets
    ) = 0;

    [[nodiscard]] virtual std::string description() const = 0;
};

}  // namespace companionlab::application::ports
