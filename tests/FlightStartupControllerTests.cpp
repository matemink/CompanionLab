#include "TestCases.hpp"

#include "onboard_autonomy/application/FlightStartupController.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using onboard_autonomy::application::FlightAction;
using onboard_autonomy::application::FlightActionRequest;
using onboard_autonomy::application::FlightCommandAckOutcome;
using onboard_autonomy::application::FlightStartupController;
using onboard_autonomy::application::FlightStartupPhase;
using onboard_autonomy::domain::TimePoint;
using onboard_autonomy::domain::VehicleSnapshot;

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

VehicleSnapshot ready_vehicle() {
    VehicleSnapshot vehicle;
    vehicle.connected = true;
    vehicle.gps_ready = true;
    vehicle.battery_ready = true;
    vehicle.system_health_known = true;
    vehicle.system_health_ok = true;
    vehicle.armable = true;
    vehicle.system_id = 1;
    vehicle.vehicle_type = 2;
    vehicle.autopilot_type = 3;
    vehicle.flight_mode = 0;
    vehicle.relative_altitude_m = 0.0;
    return vehicle;
}

FlightActionRequest only_action(
    const std::vector<FlightActionRequest>& actions,
    const FlightAction expected,
    const std::string& message
) {
    require(
        actions.size() == 1 && actions.front().action == expected,
        message
    );
    return actions.front();
}

void accept(
    FlightStartupController& controller,
    const FlightActionRequest& request,
    const TimePoint now
) {
    controller.on_action_sent(request, true, now);
    controller.on_command_ack(
        request.action,
        FlightCommandAckOutcome::accepted,
        0,
        1,
        now
    );
}

void startup_ends_after_verified_takeoff() {
    FlightStartupController controller{
        {.enabled = true, .takeoff_altitude_m = 8.0}
    };
    auto vehicle = ready_vehicle();
    const TimePoint start{};

    auto action = only_action(
        controller.update(vehicle, true, start),
        FlightAction::set_guided_mode,
        "startup must begin with GUIDED"
    );
    accept(controller, action, start);
    vehicle.flight_mode = 4;

    action = only_action(
        controller.update(
            vehicle,
            true,
            start + std::chrono::milliseconds(100)
        ),
        FlightAction::arm,
        "confirmed GUIDED must lead to ARM"
    );
    accept(controller, action, start + std::chrono::milliseconds(100));
    vehicle.armed = true;

    action = only_action(
        controller.update(
            vehicle,
            true,
            start + std::chrono::milliseconds(200)
        ),
        FlightAction::takeoff,
        "confirmed ARMED must lead to TAKEOFF"
    );
    require(
        action.altitude_m == 8.0,
        "startup must carry configured safe altitude"
    );
    accept(controller, action, start + std::chrono::milliseconds(200));

    vehicle.relative_altitude_m = 7.7;
    require(
        controller.update(
            vehicle,
            true,
            start + std::chrono::seconds(3)
        ).empty(),
        "altitude confirmation must not emit another command"
    );
    require(
        controller.snapshot().phase == FlightStartupPhase::completed,
        "startup responsibility must end after verified takeoff"
    );
}

void startup_fails_after_three_missing_acknowledgements() {
    FlightStartupController controller{
        {.enabled = true, .takeoff_altitude_m = 5.0}
    };
    const auto vehicle = ready_vehicle();
    const TimePoint start{};

    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto action = only_action(
            controller.update(
                vehicle,
                true,
                start + std::chrono::seconds(attempt * 3)
            ),
            FlightAction::set_guided_mode,
            "missing GUIDED ACK must retry"
        );
        require(
            action.confirmation ==
                static_cast<std::uint8_t>(attempt),
            "retry confirmation must increase"
        );
    }

    static_cast<void>(controller.update(
        vehicle,
        true,
        start + std::chrono::seconds(9)
    ));
    require(
        controller.snapshot().phase == FlightStartupPhase::failed,
        "startup must fail after ACK retry exhaustion"
    );
}

void startup_stops_on_heartbeat_loss() {
    FlightStartupController controller{
        {.enabled = true, .takeoff_altitude_m = 5.0}
    };
    auto vehicle = ready_vehicle();
    const TimePoint start{};
    static_cast<void>(controller.update(vehicle, false, start));

    vehicle.connected = false;
    static_cast<void>(controller.update(
        vehicle,
        false,
        start + std::chrono::seconds(1)
    ));
    require(
        controller.snapshot().phase == FlightStartupPhase::failed,
        "heartbeat loss must stop startup commands"
    );
}

}  // namespace

void run_flight_startup_controller_tests() {
    startup_ends_after_verified_takeoff();
    startup_fails_after_three_missing_acknowledgements();
    startup_stops_on_heartbeat_loss();
}
