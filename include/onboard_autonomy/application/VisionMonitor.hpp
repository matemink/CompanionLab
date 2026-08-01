#pragma once

#include "onboard_autonomy/application/ports/TargetDetector.hpp"
#include "onboard_autonomy/domain/VehicleState.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace onboard_autonomy::application {

struct VisionSnapshot {
    std::string detector;
    std::uint64_t processed_frames{0};
    std::uint64_t frames_with_targets{0};
    std::uint64_t total_targets{0};
    std::optional<double> latest_processing_ms;
    std::optional<double> average_processing_ms;
    std::optional<double> maximum_processing_ms;
    std::optional<double> last_detection_age_ms;
    std::vector<domain::TargetObservation> latest_targets;
};

class VisionMonitor {
public:
    explicit VisionMonitor(ports::TargetDetector& detector);
    ~VisionMonitor();

    VisionMonitor(const VisionMonitor&) = delete;
    VisionMonitor& operator=(const VisionMonitor&) = delete;
    VisionMonitor(VisionMonitor&&) noexcept;
    VisionMonitor& operator=(VisionMonitor&&) noexcept;

    const std::vector<domain::TargetObservation>& process(
        const ports::CameraFrame& frame,
        domain::TimePoint now
    );
    [[nodiscard]] VisionSnapshot snapshot(
        domain::TimePoint now
    ) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace onboard_autonomy::application
