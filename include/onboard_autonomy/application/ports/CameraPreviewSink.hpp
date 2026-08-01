#pragma once

#include "onboard_autonomy/application/TargetTracker.hpp"
#include "onboard_autonomy/application/ports/CameraSource.hpp"
#include "onboard_autonomy/domain/TargetObservation.hpp"

#include <span>
#include <string>

namespace onboard_autonomy::application::ports {

class CameraPreviewSink {
public:
    virtual ~CameraPreviewSink() = default;

    virtual void publish(
        const CameraFrame& frame,
        std::span<const domain::TargetObservation> targets,
        const TargetTrackSnapshot& target_track
    ) = 0;

    [[nodiscard]] virtual std::string description() const = 0;
};

}  // namespace onboard_autonomy::application::ports
