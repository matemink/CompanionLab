#include "onboard_autonomy/application/Scenario.hpp"

#include <array>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace onboard_autonomy::application {
namespace {

using namespace std::chrono_literals;

template <typename... Visitors>
struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

const std::array<ScenarioDefinition, 5> kScenarios{{
    {
        .id = ScenarioId::hover_check,
        .name = "HOVER CHECK",
        .summary = "TAKEOFF 5M > HOLD > LAND",
        .steps = {
            SetGuidedStep{},
            ArmStep{},
            TakeoffStep{5.0},
            HoldStep{5s},
            LandStep{},
        },
    },
    {
        .id = ScenarioId::out_and_rtl,
        .name = "OUT & RTL",
        .summary = "TAKEOFF > NORTH 15M > HOLD > RTL",
        .steps = {
            SetGuidedStep{},
            ArmStep{},
            TakeoffStep{5.0},
            MoveLocalStep{15.0, 0.0, 0.0, "NORTH 15M"},
            HoldStep{3s},
            ReturnToLaunchStep{},
        },
    },
    {
        .id = ScenarioId::square_patrol,
        .name = "SQUARE PATROL",
        .summary = "10M SQUARE > RTL",
        .steps = {
            SetGuidedStep{},
            ArmStep{},
            TakeoffStep{5.0},
            MoveLocalStep{10.0, 0.0, 0.0, "NORTH"},
            MoveLocalStep{0.0, 10.0, 0.0, "EAST"},
            MoveLocalStep{-10.0, 0.0, 0.0, "SOUTH"},
            MoveLocalStep{0.0, -10.0, 0.0, "WEST"},
            HoldStep{2s},
            ReturnToLaunchStep{},
        },
    },
    {
        .id = ScenarioId::search_grid,
        .name = "SEARCH GRID",
        .summary = "24M X 12M ZIGZAG > RTL",
        .steps = {
            SetGuidedStep{},
            ArmStep{},
            TakeoffStep{6.0},
            MoveLocalStep{8.0, -6.0, 0.0, "LEG 1"},
            MoveLocalStep{0.0, 12.0, 0.0, "LEG 2"},
            MoveLocalStep{8.0, 0.0, 0.0, "LEG 3"},
            MoveLocalStep{0.0, -12.0, 0.0, "LEG 4"},
            MoveLocalStep{8.0, 0.0, 0.0, "LEG 5"},
            MoveLocalStep{0.0, 12.0, 0.0, "LEG 6"},
            ReturnToLaunchStep{},
        },
    },
    {
        .id = ScenarioId::precision_landing,
        .name = "PRECISION LANDING",
        .summary = "MARKER APPROACH > VISION LANDING_TARGET > LAND",
        .steps = {
            SetGuidedStep{},
            ArmStep{},
            TakeoffStep{8.0},
            MoveLocalStep{3.0, 1.5, 0.0, "MARKER APPROACH"},
            HoldStep{2s},
            PrecisionLandStep{},
        },
    },
}};

}  // namespace

std::span<const ScenarioDefinition> demo_scenarios() {
    return kScenarios;
}

const ScenarioDefinition& scenario_definition(const ScenarioId id) {
    for (const auto& scenario : kScenarios) {
        if (scenario.id == id) {
            return scenario;
        }
    }
    throw std::invalid_argument("unknown demo scenario");
}

std::string scenario_step_name(const ScenarioStep& step) {
    return std::visit(
        Overloaded{
            [](const SetGuidedStep&) {
                return std::string{"SET GUIDED"};
            },
            [](const ArmStep&) {
                return std::string{"ARM"};
            },
            [](const TakeoffStep&) {
                return std::string{"TAKEOFF"};
            },
            [](const HoldStep&) {
                return std::string{"HOLD"};
            },
            [](const MoveLocalStep& move) {
                return "MOVE " + move.label;
            },
            [](const ReturnToLaunchStep&) {
                return std::string{"RTL"};
            },
            [](const LandStep&) {
                return std::string{"LAND"};
            },
            [](const PrecisionLandStep&) {
                return std::string{"PRECISION LAND"};
            },
        },
        step
    );
}

}  // namespace onboard_autonomy::application
