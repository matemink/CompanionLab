#pragma once

#include "onboard_autonomy/application/Scenario.hpp"
#include "onboard_autonomy/domain/VehicleState.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace onboard_autonomy::application {

enum class ScenarioRunnerPhase {
    disabled,
    waiting_for_vehicle,
    waiting_for_readiness,
    executing,
    completed,
    failed,
};

enum class FlightAction {
    set_guided_mode,
    arm,
    takeoff,
    move_local,
    return_to_launch,
    land,
    landing_target,
};

enum class FlightCommandAckOutcome {
    accepted,
    in_progress,
    rejected,
};

struct ScenarioRunnerConfig {
    bool enabled{false};
    ScenarioId initial_scenario{ScenarioId::hover_check};
};

struct FlightActionRequest {
    FlightAction action;
    std::uint8_t vehicle_system_id{0};
    std::uint8_t confirmation{0};
    double altitude_m{0.0};
    double x_m{0.0};
    double y_m{0.0};
    double z_m{0.0};
    std::uint64_t time_usec{0};
};

struct ScenarioSnapshot {
    ScenarioRunnerPhase phase{ScenarioRunnerPhase::disabled};
    std::optional<ScenarioId> scenario_id;
    std::string scenario_name{"NONE"};
    std::string step_name{"IDLE"};
    std::string detail{"Scenario runner disabled"};
    std::size_t current_step{0};
    std::size_t total_steps{0};
    double target_altitude_m{0.0};
    std::size_t attempt{0};
    std::optional<std::uint8_t> failure_result;
    bool synthetic_landing_target_active{false};
};

class ScenarioRunner {
public:
    explicit ScenarioRunner(ScenarioRunnerConfig config = {});

    [[nodiscard]] bool start(ScenarioId id, domain::TimePoint now);
    void cancel(std::string detail);

    [[nodiscard]] std::vector<FlightActionRequest> update(
        const domain::VehicleSnapshot& vehicle,
        bool telemetry_ready,
        domain::TimePoint now
    );

    void on_action_sent(
        const FlightActionRequest& request,
        bool sent,
        domain::TimePoint now
    );

    void on_command_ack(
        FlightAction action,
        FlightCommandAckOutcome outcome,
        std::uint8_t raw_result,
        std::uint8_t source_system,
        domain::TimePoint now
    );

    [[nodiscard]] ScenarioSnapshot snapshot() const;

private:
    struct LocalPosition {
        double north_m;
        double east_m;
        double down_m;
    };

    void reset_execution_state();
    void enter_waiting_for_vehicle(domain::TimePoint now);
    void enter_current_step(
        const domain::VehicleSnapshot& vehicle,
        domain::TimePoint now
    );
    void advance_step(
        const domain::VehicleSnapshot& vehicle,
        domain::TimePoint now
    );
    void fail(std::string detail);
    void update_readiness_detail(
        const domain::VehicleSnapshot& vehicle,
        bool telemetry_ready
    );

    [[nodiscard]] std::optional<FlightActionRequest> update_command(
        FlightAction action,
        domain::TimePoint now
    );
    [[nodiscard]] std::optional<FlightActionRequest> update_move(
        const MoveLocalStep& move,
        const domain::VehicleSnapshot& vehicle,
        domain::TimePoint now
    );
    [[nodiscard]] std::vector<FlightActionRequest>
    update_precision_land(
        const domain::VehicleSnapshot& vehicle,
        domain::TimePoint now
    );

    ScenarioRunnerConfig config_;
    ScenarioRunnerPhase phase_{ScenarioRunnerPhase::disabled};
    const ScenarioDefinition* scenario_{nullptr};
    std::size_t step_index_{0};
    std::string detail_{"Scenario runner disabled"};
    std::optional<std::uint8_t> vehicle_system_id_;
    std::optional<LocalPosition> home_position_;
    std::optional<LocalPosition> move_target_;
    std::size_t attempt_{0};
    bool action_sent_{false};
    bool awaiting_ack_{false};
    bool command_accepted_{false};
    bool synthetic_landing_target_active_{false};
    domain::TimePoint acknowledgement_deadline_{};
    domain::TimePoint phase_deadline_{};
    domain::TimePoint hold_deadline_{};
    domain::TimePoint next_landing_target_{};
    domain::TimePoint precision_land_command_after_{};
    std::optional<std::uint8_t> failure_result_;
};

}  // namespace onboard_autonomy::application
