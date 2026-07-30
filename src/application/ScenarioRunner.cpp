#include "companionlab/application/ScenarioRunner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace companionlab::application {
namespace {

constexpr std::uint8_t kArduPilotAutopilotType = 3;
constexpr std::uint32_t kCopterGuidedMode = 4;
constexpr std::uint32_t kCopterRtlMode = 6;
constexpr std::size_t kMaximumActionAttempts = 3;
constexpr auto kAcknowledgementTimeout = std::chrono::seconds(2);
constexpr auto kReadinessTimeout = std::chrono::seconds(90);
constexpr auto kModeChangeTimeout = std::chrono::seconds(10);
constexpr auto kArmingTimeout = std::chrono::seconds(10);
constexpr auto kTakeoffTimeout = std::chrono::seconds(45);
constexpr auto kMoveTimeout = std::chrono::seconds(60);
constexpr auto kLandingTimeout = std::chrono::seconds(90);
constexpr auto kRtlTimeout = std::chrono::seconds(120);
constexpr auto kLandingTargetInterval =
    std::chrono::milliseconds(200);
constexpr auto kLandingTargetWarmup = std::chrono::seconds(1);
constexpr double kAltitudeToleranceM = 0.35;
constexpr double kPositionToleranceM = 0.75;
constexpr double kPrecisionTargetStopAltitudeM = 0.20;

bool is_multicopter(const std::optional<std::uint8_t> vehicle_type) {
    if (!vehicle_type.has_value()) {
        return false;
    }

    switch (*vehicle_type) {
        case 2:   // MAV_TYPE_QUADROTOR
        case 3:   // MAV_TYPE_COAXIAL
        case 4:   // MAV_TYPE_HELICOPTER
        case 13:  // MAV_TYPE_HEXAROTOR
        case 14:  // MAV_TYPE_OCTOROTOR
        case 15:  // MAV_TYPE_TRICOPTER
            return true;
        default:
            return false;
    }
}

bool scenario_needs_local_position(
    const ScenarioDefinition& scenario
) {
    return std::any_of(
        scenario.steps.begin(),
        scenario.steps.end(),
        [](const ScenarioStep& step) {
            return std::holds_alternative<MoveLocalStep>(step) ||
                   std::holds_alternative<PrecisionLandStep>(step);
        }
    );
}

bool scenario_needs_attitude(const ScenarioDefinition& scenario) {
    return std::any_of(
        scenario.steps.begin(),
        scenario.steps.end(),
        [](const ScenarioStep& step) {
            return std::holds_alternative<PrecisionLandStep>(step);
        }
    );
}

bool has_local_position(const domain::VehicleSnapshot& vehicle) {
    return vehicle.local_north_m.has_value() &&
           vehicle.local_east_m.has_value() &&
           vehicle.local_down_m.has_value();
}

bool has_attitude(const domain::VehicleSnapshot& vehicle) {
    return vehicle.roll_rad.has_value() &&
           vehicle.pitch_rad.has_value() &&
           vehicle.yaw_rad.has_value();
}

double target_altitude(const ScenarioDefinition& scenario) {
    for (const auto& step : scenario.steps) {
        if (const auto* takeoff = std::get_if<TakeoffStep>(&step)) {
            return takeoff->altitude_m;
        }
    }
    return 0.0;
}

std::string altitude_detail(
    const std::optional<double> altitude_m,
    const double target_m
) {
    std::ostringstream output;
    if (altitude_m.has_value()) {
        output << std::fixed << std::setprecision(2)
               << *altitude_m << " m";
    } else {
        output << "waiting for altitude";
    }
    output << " / target " << target_m << " m";
    return output.str();
}

std::string position_detail(
    const double distance_m,
    const std::string& label
) {
    std::ostringstream output;
    output << label << ": " << std::fixed << std::setprecision(2)
           << distance_m << " m remaining";
    return output.str();
}

std::string action_name(const FlightAction action) {
    switch (action) {
        case FlightAction::set_guided_mode:
            return "GUIDED mode";
        case FlightAction::arm:
            return "arm";
        case FlightAction::takeoff:
            return "takeoff";
        case FlightAction::move_local:
            return "local position target";
        case FlightAction::return_to_launch:
            return "RTL";
        case FlightAction::land:
            return "land";
        case FlightAction::landing_target:
            return "landing target";
    }
    return "unknown action";
}

std::optional<FlightAction> command_for_step(
    const ScenarioStep& step
) {
    if (std::holds_alternative<SetGuidedStep>(step)) {
        return FlightAction::set_guided_mode;
    }
    if (std::holds_alternative<ArmStep>(step)) {
        return FlightAction::arm;
    }
    if (std::holds_alternative<TakeoffStep>(step)) {
        return FlightAction::takeoff;
    }
    if (std::holds_alternative<ReturnToLaunchStep>(step)) {
        return FlightAction::return_to_launch;
    }
    if (std::holds_alternative<LandStep>(step) ||
        std::holds_alternative<PrecisionLandStep>(step)) {
        return FlightAction::land;
    }
    return std::nullopt;
}

}  // namespace

ScenarioRunner::ScenarioRunner(ScenarioRunnerConfig config)
    : config_(std::move(config)) {
    if (config_.enabled) {
        scenario_ = &scenario_definition(config_.initial_scenario);
        phase_ = ScenarioRunnerPhase::waiting_for_vehicle;
        detail_ = "Waiting for the flight-controller heartbeat";
    }
}

bool ScenarioRunner::start(
    const ScenarioId id,
    const domain::TimePoint now
) {
    if (phase_ != ScenarioRunnerPhase::disabled &&
        phase_ != ScenarioRunnerPhase::completed &&
        phase_ != ScenarioRunnerPhase::failed) {
        return false;
    }

    scenario_ = &scenario_definition(id);
    config_.enabled = true;
    reset_execution_state();
    enter_waiting_for_vehicle(now);
    return true;
}

void ScenarioRunner::cancel(std::string detail) {
    config_.enabled = false;
    phase_ = ScenarioRunnerPhase::disabled;
    detail_ = std::move(detail);
    reset_execution_state();
    scenario_ = nullptr;
}

std::vector<FlightActionRequest> ScenarioRunner::update(
    const domain::VehicleSnapshot& vehicle,
    const bool telemetry_ready,
    const domain::TimePoint now
) {
    std::vector<FlightActionRequest> actions;
    if (phase_ == ScenarioRunnerPhase::disabled ||
        phase_ == ScenarioRunnerPhase::completed ||
        phase_ == ScenarioRunnerPhase::failed) {
        return actions;
    }

    if (phase_ != ScenarioRunnerPhase::waiting_for_vehicle &&
        !vehicle.connected) {
        fail("Flight-controller heartbeat was lost");
        return actions;
    }

    for (int transition = 0; transition < 12; ++transition) {
        if (phase_ == ScenarioRunnerPhase::waiting_for_vehicle) {
            if (!vehicle.connected || !vehicle.system_id.has_value()) {
                return actions;
            }
            if (vehicle.autopilot_type !=
                    kArduPilotAutopilotType ||
                !is_multicopter(vehicle.vehicle_type)) {
                fail(
                    "Demo scenarios require an ArduPilot multicopter"
                );
                return actions;
            }

            vehicle_system_id_ = vehicle.system_id;
            phase_ = ScenarioRunnerPhase::waiting_for_readiness;
            phase_deadline_ = now + kReadinessTimeout;
            detail_ = "Waiting for telemetry and pre-arm readiness";
            continue;
        }

        if (phase_ == ScenarioRunnerPhase::waiting_for_readiness) {
            if (now >= phase_deadline_) {
                fail("Vehicle did not become ready within 90 seconds");
                return actions;
            }

            const bool local_position_ready =
                scenario_ == nullptr ||
                !scenario_needs_local_position(*scenario_) ||
                has_local_position(vehicle);
            const bool attitude_ready =
                scenario_ == nullptr ||
                !scenario_needs_attitude(*scenario_) ||
                has_attitude(vehicle);

            if (telemetry_ready && vehicle.armable &&
                !vehicle.armed &&
                vehicle.relative_altitude_m.has_value() &&
                local_position_ready && attitude_ready) {
                if (has_local_position(vehicle)) {
                    home_position_ = LocalPosition{
                        .north_m = *vehicle.local_north_m,
                        .east_m = *vehicle.local_east_m,
                        .down_m = *vehicle.local_down_m,
                    };
                }
                phase_ = ScenarioRunnerPhase::executing;
                step_index_ = 0;
                enter_current_step(vehicle, now);
                continue;
            }

            update_readiness_detail(vehicle, telemetry_ready);
            return actions;
        }

        if (phase_ == ScenarioRunnerPhase::completed ||
            phase_ == ScenarioRunnerPhase::failed) {
            return actions;
        }

        if (phase_ != ScenarioRunnerPhase::executing ||
            scenario_ == nullptr ||
            step_index_ >= scenario_->steps.size()) {
            fail("Scenario runner entered an invalid state");
            return actions;
        }

        const auto& step = scenario_->steps.at(step_index_);

        if (std::holds_alternative<SetGuidedStep>(step)) {
            if (command_accepted_ &&
                vehicle.flight_mode == kCopterGuidedMode) {
                advance_step(vehicle, now);
                continue;
            }
            if (now >= phase_deadline_) {
                fail("ArduCopter did not enter GUIDED mode");
                return actions;
            }
            if (auto request = update_command(
                    FlightAction::set_guided_mode,
                    now
                )) {
                actions.push_back(*request);
            }
            return actions;
        }

        if (std::holds_alternative<ArmStep>(step)) {
            if (command_accepted_ && vehicle.armed) {
                advance_step(vehicle, now);
                continue;
            }
            if (now >= phase_deadline_) {
                fail("ArduCopter did not become armed");
                return actions;
            }
            if (auto request = update_command(
                    FlightAction::arm,
                    now
                )) {
                actions.push_back(*request);
            }
            return actions;
        }

        if (const auto* takeoff =
                std::get_if<TakeoffStep>(&step)) {
            if (!vehicle.armed && command_accepted_) {
                fail("Vehicle disarmed during takeoff");
                return actions;
            }
            if (command_accepted_ &&
                vehicle.relative_altitude_m.has_value() &&
                *vehicle.relative_altitude_m >=
                    takeoff->altitude_m - kAltitudeToleranceM) {
                advance_step(vehicle, now);
                continue;
            }
            if (now >= phase_deadline_) {
                fail("Vehicle did not reach takeoff altitude");
                return actions;
            }
            if (command_accepted_) {
                detail_ = "Climbing: " + altitude_detail(
                    vehicle.relative_altitude_m,
                    takeoff->altitude_m
                );
            }
            if (auto request = update_command(
                    FlightAction::takeoff,
                    now
                )) {
                request->altitude_m = takeoff->altitude_m;
                actions.push_back(*request);
            }
            return actions;
        }

        if (const auto* hold = std::get_if<HoldStep>(&step)) {
            static_cast<void>(hold);
            if (!vehicle.armed) {
                fail("Vehicle disarmed while holding altitude");
                return actions;
            }
            detail_ = "Holding: " + altitude_detail(
                vehicle.relative_altitude_m,
                target_altitude(*scenario_)
            );
            if (now >= hold_deadline_) {
                advance_step(vehicle, now);
                continue;
            }
            return actions;
        }

        if (const auto* move =
                std::get_if<MoveLocalStep>(&step)) {
            if (!vehicle.armed) {
                fail("Vehicle disarmed during route");
                return actions;
            }
            if (action_sent_ && move_target_.has_value() &&
                has_local_position(vehicle)) {
                const double north_error =
                    *vehicle.local_north_m -
                    move_target_->north_m;
                const double east_error =
                    *vehicle.local_east_m -
                    move_target_->east_m;
                const double down_error =
                    *vehicle.local_down_m -
                    move_target_->down_m;
                const double distance = std::sqrt(
                    north_error * north_error +
                    east_error * east_error +
                    down_error * down_error
                );
                detail_ = position_detail(distance, move->label);
                if (distance <= kPositionToleranceM) {
                    advance_step(vehicle, now);
                    continue;
                }
            }
            if (auto request = update_move(*move, vehicle, now)) {
                actions.push_back(*request);
            }
            return actions;
        }

        if (std::holds_alternative<ReturnToLaunchStep>(step)) {
            if (command_accepted_ && !vehicle.armed) {
                advance_step(vehicle, now);
                continue;
            }
            if (now >= phase_deadline_) {
                fail("Vehicle did not complete RTL");
                return actions;
            }
            if (command_accepted_) {
                detail_ =
                    vehicle.flight_mode == kCopterRtlMode
                        ? "RTL active; waiting for landing and disarm"
                        : "RTL accepted; verifying flight mode";
            }
            if (auto request = update_command(
                    FlightAction::return_to_launch,
                    now
                )) {
                actions.push_back(*request);
            }
            return actions;
        }

        if (std::holds_alternative<LandStep>(step)) {
            if (command_accepted_ && !vehicle.armed) {
                advance_step(vehicle, now);
                continue;
            }
            if (now >= phase_deadline_) {
                fail("Vehicle did not complete landing");
                return actions;
            }
            if (command_accepted_) {
                detail_ = "Descending: " + altitude_detail(
                    vehicle.relative_altitude_m,
                    0.0
                );
            }
            if (auto request = update_command(
                    FlightAction::land,
                    now
                )) {
                actions.push_back(*request);
            }
            return actions;
        }

        if (std::holds_alternative<PrecisionLandStep>(step)) {
            if (command_accepted_ && !vehicle.armed) {
                advance_step(vehicle, now);
                continue;
            }
            return update_precision_land(vehicle, now);
        }
    }

    fail("Scenario made too many immediate transitions");
    return actions;
}

void ScenarioRunner::on_action_sent(
    const FlightActionRequest& request,
    const bool sent,
    const domain::TimePoint
) {
    if (request.action == FlightAction::landing_target) {
        synthetic_landing_target_active_ =
            synthetic_landing_target_active_ || sent;
        if (!sent) {
            detail_ = "Failed to send synthetic LANDING_TARGET";
        }
        return;
    }

    if (request.action == FlightAction::move_local) {
        if (!sent) {
            action_sent_ = false;
            if (attempt_ >= kMaximumActionAttempts) {
                fail("Failed to send local position target");
            }
        }
        return;
    }

    if (!sent) {
        awaiting_ack_ = false;
        if (attempt_ >= kMaximumActionAttempts) {
            fail("Failed to send " + action_name(request.action));
        }
    }
}

void ScenarioRunner::on_command_ack(
    const FlightAction action,
    const FlightCommandAckOutcome outcome,
    const std::uint8_t raw_result,
    const std::uint8_t source_system,
    const domain::TimePoint now
) {
    if (phase_ != ScenarioRunnerPhase::executing ||
        scenario_ == nullptr ||
        step_index_ >= scenario_->steps.size()) {
        return;
    }

    const auto expected =
        command_for_step(scenario_->steps.at(step_index_));
    if (!expected.has_value() || *expected != action ||
        !awaiting_ack_ || !vehicle_system_id_.has_value() ||
        source_system != *vehicle_system_id_) {
        return;
    }

    switch (outcome) {
        case FlightCommandAckOutcome::accepted:
            awaiting_ack_ = false;
            command_accepted_ = true;
            detail_ = action_name(action) +
                      " accepted; verifying vehicle state";
            break;
        case FlightCommandAckOutcome::in_progress:
            acknowledgement_deadline_ =
                now + kAcknowledgementTimeout;
            detail_ = action_name(action) + " is in progress";
            break;
        case FlightCommandAckOutcome::rejected:
            failure_result_ = raw_result;
            fail(
                action_name(action) +
                " was rejected with MAV_RESULT " +
                std::to_string(raw_result)
            );
            break;
    }
}

ScenarioSnapshot ScenarioRunner::snapshot() const {
    ScenarioSnapshot result{
        .phase = phase_,
        .scenario_id =
            scenario_ == nullptr
                ? std::nullopt
                : std::optional<ScenarioId>{scenario_->id},
        .scenario_name =
            scenario_ == nullptr ? "NONE" : scenario_->name,
        .step_name = "IDLE",
        .detail = detail_,
        .current_step =
            phase_ == ScenarioRunnerPhase::executing
                ? step_index_ + 1
                : 0,
        .total_steps =
            scenario_ == nullptr ? 0 : scenario_->steps.size(),
        .target_altitude_m =
            scenario_ == nullptr ? 0.0 : target_altitude(*scenario_),
        .attempt = attempt_,
        .failure_result = failure_result_,
        .synthetic_landing_target_active =
            synthetic_landing_target_active_,
    };

    if (scenario_ != nullptr &&
        step_index_ < scenario_->steps.size() &&
        phase_ == ScenarioRunnerPhase::executing) {
        result.step_name =
            scenario_step_name(scenario_->steps.at(step_index_));
    } else if (phase_ == ScenarioRunnerPhase::completed) {
        result.step_name = "COMPLETE";
    } else if (phase_ == ScenarioRunnerPhase::failed) {
        result.step_name = "FAILED";
    } else if (phase_ == ScenarioRunnerPhase::waiting_for_vehicle ||
               phase_ ==
                   ScenarioRunnerPhase::waiting_for_readiness) {
        result.step_name = "PREFLIGHT";
    }

    return result;
}

void ScenarioRunner::reset_execution_state() {
    step_index_ = 0;
    vehicle_system_id_.reset();
    home_position_.reset();
    move_target_.reset();
    attempt_ = 0;
    action_sent_ = false;
    awaiting_ack_ = false;
    command_accepted_ = false;
    synthetic_landing_target_active_ = false;
    acknowledgement_deadline_ = {};
    phase_deadline_ = {};
    hold_deadline_ = {};
    next_landing_target_ = {};
    precision_land_command_after_ = {};
    failure_result_.reset();
}

void ScenarioRunner::enter_waiting_for_vehicle(
    const domain::TimePoint
) {
    phase_ = ScenarioRunnerPhase::waiting_for_vehicle;
    detail_ = "Waiting for the flight-controller heartbeat";
}

void ScenarioRunner::enter_current_step(
    const domain::VehicleSnapshot& vehicle,
    const domain::TimePoint now
) {
    if (scenario_ == nullptr ||
        step_index_ >= scenario_->steps.size()) {
        fail("Scenario has no executable step");
        return;
    }

    attempt_ = 0;
    action_sent_ = false;
    awaiting_ack_ = false;
    command_accepted_ = false;
    move_target_.reset();

    const auto& step = scenario_->steps.at(step_index_);
    detail_ = scenario_step_name(step);

    if (std::holds_alternative<SetGuidedStep>(step)) {
        phase_deadline_ = now + kModeChangeTimeout;
        return;
    }
    if (std::holds_alternative<ArmStep>(step)) {
        phase_deadline_ = now + kArmingTimeout;
        return;
    }
    if (std::holds_alternative<TakeoffStep>(step)) {
        phase_deadline_ = now + kTakeoffTimeout;
        return;
    }
    if (const auto* hold = std::get_if<HoldStep>(&step)) {
        hold_deadline_ = now + hold->duration;
        return;
    }
    if (const auto* move = std::get_if<MoveLocalStep>(&step)) {
        if (!has_local_position(vehicle)) {
            fail("LOCAL_POSITION_NED is unavailable");
            return;
        }
        move_target_ = LocalPosition{
            .north_m = *vehicle.local_north_m + move->north_m,
            .east_m = *vehicle.local_east_m + move->east_m,
            .down_m = *vehicle.local_down_m + move->down_m,
        };
        phase_deadline_ = now + kMoveTimeout;
        return;
    }
    if (std::holds_alternative<ReturnToLaunchStep>(step)) {
        phase_deadline_ = now + kRtlTimeout;
        return;
    }
    if (std::holds_alternative<LandStep>(step)) {
        phase_deadline_ = now + kLandingTimeout;
        return;
    }
    if (std::holds_alternative<PrecisionLandStep>(step)) {
        phase_deadline_ = now + kLandingTimeout;
        next_landing_target_ = now;
        precision_land_command_after_ =
            now + kLandingTargetWarmup;
    }
}

void ScenarioRunner::advance_step(
    const domain::VehicleSnapshot& vehicle,
    const domain::TimePoint now
) {
    ++step_index_;
    if (scenario_ == nullptr ||
        step_index_ >= scenario_->steps.size()) {
        phase_ = ScenarioRunnerPhase::completed;
        detail_ = "Landed and disarmed";
        attempt_ = 0;
        action_sent_ = false;
        awaiting_ack_ = false;
        command_accepted_ = false;
        return;
    }
    enter_current_step(vehicle, now);
}

void ScenarioRunner::fail(std::string detail) {
    phase_ = ScenarioRunnerPhase::failed;
    detail_ = std::move(detail);
    action_sent_ = false;
    awaiting_ack_ = false;
}

void ScenarioRunner::update_readiness_detail(
    const domain::VehicleSnapshot& vehicle,
    const bool telemetry_ready
) {
    if (!telemetry_ready) {
        detail_ = "Waiting for telemetry stream setup";
    } else if (vehicle.armed) {
        detail_ = "Waiting for the vehicle to be disarmed";
    } else if (!vehicle.gps_ready) {
        detail_ = "Waiting for a 3D GPS fix";
    } else if (!vehicle.battery_ready) {
        detail_ = "Waiting for valid battery data";
    } else if (!vehicle.system_health_ok) {
        detail_ = "Waiting for healthy onboard sensors";
    } else if (!vehicle.armable) {
        detail_ = "Waiting for active PreArm warnings to clear";
    } else if (!vehicle.relative_altitude_m.has_value()) {
        detail_ = "Waiting for relative altitude";
    } else if (scenario_ != nullptr &&
               scenario_needs_local_position(*scenario_) &&
               !has_local_position(vehicle)) {
        detail_ = "Waiting for LOCAL_POSITION_NED";
    } else if (scenario_ != nullptr &&
               scenario_needs_attitude(*scenario_) &&
               !has_attitude(vehicle)) {
        detail_ = "Waiting for ATTITUDE";
    } else {
        detail_ = "Checking readiness";
    }
}

std::optional<FlightActionRequest>
ScenarioRunner::update_command(
    const FlightAction action,
    const domain::TimePoint now
) {
    if (command_accepted_) {
        return std::nullopt;
    }

    if (awaiting_ack_) {
        if (now < acknowledgement_deadline_) {
            return std::nullopt;
        }
        awaiting_ack_ = false;
    }

    if (attempt_ >= kMaximumActionAttempts) {
        fail(
            "No COMMAND_ACK for " + action_name(action) +
            " after 3 attempts"
        );
        return std::nullopt;
    }

    const auto confirmation = static_cast<std::uint8_t>(attempt_);
    ++attempt_;
    awaiting_ack_ = true;
    acknowledgement_deadline_ = now + kAcknowledgementTimeout;
    detail_ = "Sending " + action_name(action) + " | attempt " +
              std::to_string(attempt_) + "/3";

    return FlightActionRequest{
        .action = action,
        .vehicle_system_id = *vehicle_system_id_,
        .confirmation = confirmation,
    };
}

std::optional<FlightActionRequest> ScenarioRunner::update_move(
    const MoveLocalStep& move,
    const domain::VehicleSnapshot& vehicle,
    const domain::TimePoint now
) {
    if (now >= phase_deadline_) {
        fail("Vehicle did not reach route target " + move.label);
        return std::nullopt;
    }
    if (!has_local_position(vehicle)) {
        detail_ = "Waiting for LOCAL_POSITION_NED";
        return std::nullopt;
    }
    if (action_sent_) {
        return std::nullopt;
    }
    if (attempt_ >= kMaximumActionAttempts) {
        fail("Failed to send route target " + move.label);
        return std::nullopt;
    }

    ++attempt_;
    action_sent_ = true;
    detail_ = "Sending route target " + move.label;
    return FlightActionRequest{
        .action = FlightAction::move_local,
        .vehicle_system_id = *vehicle_system_id_,
        .x_m = move.north_m,
        .y_m = move.east_m,
        .z_m = move.down_m,
    };
}

std::vector<FlightActionRequest>
ScenarioRunner::update_precision_land(
    const domain::VehicleSnapshot& vehicle,
    const domain::TimePoint now
) {
    std::vector<FlightActionRequest> actions;
    if (now >= phase_deadline_) {
        fail("Vehicle did not complete precision landing");
        return actions;
    }
    if (!home_position_.has_value() ||
        !has_local_position(vehicle) ||
        !has_attitude(vehicle)) {
        detail_ = "Waiting for precision target pose";
        return actions;
    }

    if (command_accepted_ &&
        vehicle.relative_altitude_m.has_value() &&
        *vehicle.relative_altitude_m <=
            kPrecisionTargetStopAltitudeM) {
        synthetic_landing_target_active_ = false;
        detail_ =
            "Touchdown detected; waiting for ArduPilot auto-disarm";
        return actions;
    }

    const double north =
        home_position_->north_m - *vehicle.local_north_m;
    const double east =
        home_position_->east_m - *vehicle.local_east_m;
    const double down =
        home_position_->down_m - *vehicle.local_down_m;

    const double roll = *vehicle.roll_rad;
    const double pitch = *vehicle.pitch_rad;
    const double yaw = *vehicle.yaw_rad;
    const double cr = std::cos(roll);
    const double sr = std::sin(roll);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);

    // Transpose of the body-to-NED attitude matrix.
    const double forward =
        cp * cy * north + cp * sy * east - sp * down;
    const double right =
        (sr * sp * cy - cr * sy) * north +
        (sr * sp * sy + cr * cy) * east +
        sr * cp * down;
    const double body_down =
        (cr * sp * cy + sr * sy) * north +
        (cr * sp * sy - sr * cy) * east +
        cr * cp * down;

    if (now >= next_landing_target_) {
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::microseconds
        >(now.time_since_epoch());
        actions.push_back(
            FlightActionRequest{
                .action = FlightAction::landing_target,
                .vehicle_system_id = *vehicle_system_id_,
                .x_m = forward,
                .y_m = right,
                .z_m = body_down,
                .time_usec =
                    static_cast<std::uint64_t>(elapsed.count()),
            }
        );
        next_landing_target_ = now + kLandingTargetInterval;
    }

    if (!command_accepted_ &&
        now >= precision_land_command_after_) {
        if (auto land = update_command(FlightAction::land, now)) {
            actions.push_back(*land);
        }
    }

    std::ostringstream target;
    target << "Synthetic target F/R/D "
           << std::fixed << std::setprecision(1)
           << forward << "/" << right << "/" << body_down << " m";
    detail_ = target.str();
    return actions;
}

}  // namespace companionlab::application
