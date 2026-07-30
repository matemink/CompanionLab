#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace companionlab::domain {

struct ImagePoint {
    double x_px{0.0};
    double y_px{0.0};
};

struct TargetObservation {
    std::int32_t id{0};
    std::string family;
    ImagePoint center;
    std::array<ImagePoint, 4> corners;
    std::int32_t corrected_bits{0};
    double decision_margin{0.0};
};

struct TargetDetectionBatch {
    std::uint64_t frame_sequence{0};
    std::optional<std::chrono::system_clock::time_point> captured_at;
    std::chrono::system_clock::time_point detected_at;
    std::chrono::microseconds processing_time{};
    std::vector<TargetObservation> targets;
};

}  // namespace companionlab::domain
