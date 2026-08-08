#pragma once

#include "onboard_autonomy/domain/TargetTransform.hpp"
#include "onboard_autonomy/domain/VehicleState.hpp"

#include <cstdint>
#include <optional>

namespace onboard_autonomy::application {

struct WorldState {
    bool flight_controller_connected{false};
    bool vehicle_armed{false};
    std::optional<std::uint8_t> vehicle_system_id;
    std::optional<domain::BodyFramePosition> landing_target;
    domain::TimePoint observed_at;
};

[[nodiscard]] WorldState make_world_state(
    const domain::VehicleSnapshot& vehicle,
    std::optional<domain::BodyFramePosition> landing_target,
    domain::TimePoint observed_at
);

}  // namespace onboard_autonomy::application
