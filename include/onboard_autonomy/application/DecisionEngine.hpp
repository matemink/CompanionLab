#pragma once

#include "onboard_autonomy/application/WorldState.hpp"

#include <chrono>
#include <optional>

namespace onboard_autonomy::application {

enum class DesiredMotionType {
    precision_land,
};

struct DesiredMotion {
    DesiredMotionType type{DesiredMotionType::precision_land};
    std::uint8_t vehicle_system_id{0};
    domain::BodyFramePosition landing_target;
    domain::TimePoint created_at;
    domain::TimePoint valid_until;
};

class DecisionEngine {
public:
    [[nodiscard]] std::optional<DesiredMotion> decide(
        const WorldState& world
    ) const;

private:
    static constexpr auto kIntentLifetime =
        std::chrono::milliseconds(250);
};

}  // namespace onboard_autonomy::application
