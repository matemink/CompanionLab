#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace companionlab::application {

enum class ScenarioId : std::uint8_t {
    hover_check = 1,
    out_and_rtl = 2,
    square_patrol = 3,
    search_grid = 4,
    precision_landing = 5,
};

struct SetGuidedStep {};
struct ArmStep {};

struct TakeoffStep {
    double altitude_m;
};

struct HoldStep {
    std::chrono::seconds duration;
};

struct MoveLocalStep {
    double north_m;
    double east_m;
    double down_m;
    std::string label;
};

struct ReturnToLaunchStep {};
struct LandStep {};
struct PrecisionLandStep {};

using ScenarioStep = std::variant<
    SetGuidedStep,
    ArmStep,
    TakeoffStep,
    HoldStep,
    MoveLocalStep,
    ReturnToLaunchStep,
    LandStep,
    PrecisionLandStep
>;

struct ScenarioDefinition {
    ScenarioId id;
    std::string name;
    std::string summary;
    std::vector<ScenarioStep> steps;
};

[[nodiscard]] std::span<const ScenarioDefinition> demo_scenarios();

[[nodiscard]] const ScenarioDefinition& scenario_definition(
    ScenarioId id
);

[[nodiscard]] std::string scenario_step_name(
    const ScenarioStep& step
);

}  // namespace companionlab::application
