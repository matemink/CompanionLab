#include "TestCases.hpp"

#include "onboard_autonomy/application/ScenarioRunner.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

using onboard_autonomy::application::FlightAction;
using onboard_autonomy::application::FlightActionRequest;
using onboard_autonomy::application::FlightCommandAckOutcome;
using onboard_autonomy::application::ScenarioId;
using onboard_autonomy::application::ScenarioRunner;
using onboard_autonomy::application::ScenarioRunnerPhase;
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
    vehicle.armed = false;
    vehicle.system_id = 1;
    vehicle.vehicle_type = 2;
    vehicle.autopilot_type = 3;
    vehicle.flight_mode = 0;
    vehicle.relative_altitude_m = 0.0;
    vehicle.local_north_m = 0.0;
    vehicle.local_east_m = 0.0;
    vehicle.local_down_m = 0.0;
    vehicle.roll_rad = 0.0;
    vehicle.pitch_rad = 0.0;
    vehicle.yaw_rad = 0.0;
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
    ScenarioRunner& runner,
    const FlightActionRequest& request,
    const TimePoint now
) {
    runner.on_action_sent(request, true, now);
    runner.on_command_ack(
        request.action,
        FlightCommandAckOutcome::accepted,
        0,
        1,
        now
    );
}

void reach_takeoff(
    ScenarioRunner& runner,
    VehicleSnapshot& vehicle,
    const TimePoint start
) {
    auto action = only_action(
        runner.update(vehicle, true, start),
        FlightAction::set_guided_mode,
        "scenario must begin with GUIDED"
    );
    accept(runner, action, start);
    vehicle.flight_mode = 4;

    action = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::milliseconds(100)
        ),
        FlightAction::arm,
        "ARM must follow confirmed GUIDED"
    );
    accept(runner, action, start + std::chrono::milliseconds(100));
    vehicle.armed = true;

    action = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::milliseconds(200)
        ),
        FlightAction::takeoff,
        "TAKEOFF must follow confirmed ARMED"
    );
    accept(runner, action, start + std::chrono::milliseconds(200));
}

void catalog_contains_five_typed_scenarios() {
    const auto scenarios =
        onboard_autonomy::application::demo_scenarios();
    require(scenarios.size() == 5, "demo catalog must contain five entries");

    for (std::size_t index = 0; index < scenarios.size(); ++index) {
        require(
            static_cast<std::size_t>(scenarios[index].id) ==
                index + 1,
            "scenario IDs must map directly to keys 1..5"
        );
        require(
            !scenarios[index].steps.empty(),
            "every scenario must contain executable steps"
        );
    }

    const auto& precision =
        onboard_autonomy::application::scenario_definition(
            ScenarioId::precision_landing
        );
    require(
        std::any_of(
            precision.steps.begin(),
            precision.steps.end(),
            [](const auto& step) {
                return std::holds_alternative<
                    onboard_autonomy::application::PrecisionLandStep
                >(step);
            }
        ),
        "scenario 5 must contain a real precision-land step"
    );
}

void hover_check_follows_verified_vehicle_state() {
    ScenarioRunner runner{
        {
            .enabled = true,
            .initial_scenario = ScenarioId::hover_check,
        }
    };
    auto vehicle = ready_vehicle();
    const TimePoint start{};
    reach_takeoff(runner, vehicle, start);

    vehicle.relative_altitude_m = 5.0;
    require(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(3)
        ).empty(),
        "reaching altitude must enter command-free HOLD"
    );
    require(
        runner.snapshot().step_name == "HOLD",
        "altitude telemetry must advance to HOLD"
    );

    const auto land = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(8)
        ),
        FlightAction::land,
        "hold deadline must trigger LAND"
    );
    accept(runner, land, start + std::chrono::seconds(8));
    vehicle.armed = false;
    vehicle.relative_altitude_m = 0.0;
    require(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(10)
        ).empty(),
        "completed landing must not emit another action"
    );
    require(
        runner.snapshot().phase == ScenarioRunnerPhase::completed,
        "scenario completes only after DISARMED telemetry"
    );
}

void out_and_rtl_uses_position_feedback() {
    ScenarioRunner runner{
        {
            .enabled = true,
            .initial_scenario = ScenarioId::out_and_rtl,
        }
    };
    auto vehicle = ready_vehicle();
    const TimePoint start{};
    reach_takeoff(runner, vehicle, start);

    vehicle.relative_altitude_m = 5.0;
    vehicle.local_down_m = -5.0;
    const auto move = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(3)
        ),
        FlightAction::move_local,
        "OUT & RTL must send its local position target"
    );
    require(
        move.x_m == 15.0 && move.y_m == 0.0 && move.z_m == 0.0,
        "scenario 2 must fly 15 m north"
    );
    runner.on_action_sent(move, true, start + std::chrono::seconds(3));

    vehicle.local_north_m = 15.0;
    require(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(4)
        ).empty(),
        "reaching the route target must enter HOLD"
    );

    const auto rtl = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(7)
        ),
        FlightAction::return_to_launch,
        "route completion must trigger RTL"
    );
    accept(runner, rtl, start + std::chrono::seconds(7));
    vehicle.flight_mode = 6;
    vehicle.armed = false;
    static_cast<void>(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(12)
        )
    );
    require(
        runner.snapshot().phase == ScenarioRunnerPhase::completed,
        "RTL completes only after landing and disarm"
    );
}

void precision_landing_streams_target_before_land() {
    ScenarioRunner runner{
        {
            .enabled = true,
            .initial_scenario = ScenarioId::precision_landing,
        }
    };
    auto vehicle = ready_vehicle();
    const TimePoint start{};
    reach_takeoff(runner, vehicle, start);

    vehicle.relative_altitude_m = 8.0;
    vehicle.local_down_m = -8.0;
    const auto move = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(3)
        ),
        FlightAction::move_local,
        "precision scenario must first offset from home"
    );
    runner.on_action_sent(move, true, start + std::chrono::seconds(3));
    vehicle.local_north_m = 8.0;
    vehicle.local_east_m = 4.0;
    static_cast<void>(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(4)
        )
    );

    const auto first_target = only_action(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(6)
        ),
        FlightAction::landing_target,
        "precision step must warm up LANDING_TARGET before LAND"
    );
    require(
        first_target.x_m == -8.0 &&
            first_target.y_m == -4.0 &&
            first_target.z_m == 8.0,
        "synthetic target must point from vehicle to home in body FRD"
    );
    runner.on_action_sent(
        first_target,
        true,
        start + std::chrono::seconds(6)
    );

    const auto actions = runner.update(
        vehicle,
        true,
        start + std::chrono::seconds(7)
    );
    require(
        std::any_of(
            actions.begin(),
            actions.end(),
            [](const FlightActionRequest& action) {
                return action.action == FlightAction::landing_target;
            }
        ) &&
            std::any_of(
                actions.begin(),
                actions.end(),
                [](const FlightActionRequest& action) {
                    return action.action == FlightAction::land;
                }
            ),
        "precision step must keep streaming target while requesting LAND"
    );
    require(
        runner.snapshot().synthetic_landing_target_active,
        "snapshot must expose the active synthetic target source"
    );

    const auto land = std::find_if(
        actions.begin(),
        actions.end(),
        [](const FlightActionRequest& action) {
            return action.action == FlightAction::land;
        }
    );
    require(land != actions.end(), "precision LAND command is required");
    accept(runner, *land, start + std::chrono::seconds(7));

    vehicle.relative_altitude_m = 0.15;
    vehicle.local_down_m = -0.15;
    require(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(8)
        ).empty(),
        "LANDING_TARGET must stop after confirmed touchdown"
    );
    require(
        !runner.snapshot().synthetic_landing_target_active,
        "touchdown must mark the synthetic target stream inactive"
    );

    vehicle.armed = false;
    static_cast<void>(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(9)
        )
    );
    require(
        runner.snapshot().phase == ScenarioRunnerPhase::completed,
        "precision landing completes only after auto-disarm"
    );
}

void missing_ack_retries_three_times_then_fails() {
    ScenarioRunner runner{
        {
            .enabled = true,
            .initial_scenario = ScenarioId::hover_check,
        }
    };
    const auto vehicle = ready_vehicle();
    const TimePoint start{};

    for (int attempt = 0; attempt < 3; ++attempt) {
        const auto action = only_action(
            runner.update(
                vehicle,
                true,
                start + std::chrono::seconds(attempt * 3)
            ),
            FlightAction::set_guided_mode,
            "missing ACK must retry GUIDED"
        );
        require(
            action.confirmation ==
                static_cast<std::uint8_t>(attempt),
            "retry confirmation must increase"
        );
    }

    require(
        runner.update(
            vehicle,
            true,
            start + std::chrono::seconds(9)
        ).empty(),
        "retry exhaustion must stop action emission"
    );
    require(
        runner.snapshot().phase == ScenarioRunnerPhase::failed,
        "missing ACK must fail the scenario"
    );
}

}  // namespace

void run_scenario_runner_tests() {
    catalog_contains_five_typed_scenarios();
    hover_check_follows_verified_vehicle_state();
    out_and_rtl_uses_position_feedback();
    precision_landing_streams_target_before_land();
    missing_ack_retries_three_times_then_fails();
}
