#pragma once

#include "onboard_autonomy/application/DecisionEngine.hpp"

#include <optional>
#include <string_view>

namespace onboard_autonomy::application {

enum class MotionSafetyStatus {
    allowed,
    no_intent,
    flight_controller_disconnected,
    vehicle_disarmed,
    stale_intent,
    invalid_target,
};

struct SupervisedMotion {
    MotionSafetyStatus status{MotionSafetyStatus::no_intent};
    std::optional<DesiredMotion> approved;
    std::string_view detail;
};

class SafetySupervisor {
public:
    [[nodiscard]] SupervisedMotion supervise(
        const WorldState& world,
        const std::optional<DesiredMotion>& desired,
        domain::TimePoint now
    ) const;
};

}  // namespace onboard_autonomy::application
