#pragma once

#include "onboard_autonomy/application/ports/TargetDetector.hpp"

#include <cstdint>
#include <memory>

namespace onboard_autonomy::adapters::vision {

struct AprilTagDetectorConfig {
    std::uint32_t worker_threads{2};
    double quad_decimate{1.0};
    bool refine_edges{true};
    std::int32_t corrected_bits{2};
};

[[nodiscard]] std::unique_ptr<
    application::ports::TargetDetector
> make_apriltag_target_detector(
    AprilTagDetectorConfig config = {}
);

}  // namespace onboard_autonomy::adapters::vision
