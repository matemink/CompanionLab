#pragma once

#include "companionlab/application/CameraMonitor.hpp"
#include "companionlab/application/ScenarioRunner.hpp"
#include "companionlab/domain/VehicleState.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace companionlab::application {

enum class LinkEventDirection {
    outbound,
    inbound,
};

enum class LinkEventStatus {
    neutral,
    pending,
    success,
    warning,
    failure,
};

struct LinkEvent {
    std::uint64_t sequence{0};
    std::chrono::milliseconds elapsed{};
    LinkEventDirection direction{LinkEventDirection::outbound};
    LinkEventStatus status{LinkEventStatus::neutral};
    std::string label;
    std::string detail;
};

struct LinkActivity {
    std::uint64_t sequence{0};
    std::chrono::milliseconds observed_at{};
    std::string message_name;
    std::string detail;
};

enum class TelemetrySetupState {
    waiting_for_vehicle,
    configuring,
    active,
    failed,
};

struct TelemetryStatus {
    TelemetrySetupState state{
        TelemetrySetupState::waiting_for_vehicle
    };
    std::size_t completed_requests{0};
    std::size_t total_requests{0};
    std::string current_stream;
    std::size_t attempt{0};
    std::optional<std::uint8_t> failure_result;
};

struct AppSnapshot {
    domain::VehicleSnapshot vehicle;
    bool companion_heartbeat_active{false};
    TelemetryStatus telemetry;
    std::optional<CameraSnapshot> camera;
    std::optional<VisionSnapshot> vision;
    ScenarioSnapshot scenario;
    bool motion_commands_allowed{false};
    std::vector<LinkEvent> link_events;
    std::chrono::milliseconds elapsed{};
    std::optional<LinkActivity> tx_activity;
    std::optional<LinkActivity> rx_activity;

    [[nodiscard]] std::string to_json() const;
};

}  // namespace companionlab::application
