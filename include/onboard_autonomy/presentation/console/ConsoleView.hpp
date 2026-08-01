#pragma once

#include "onboard_autonomy/application/AppSnapshot.hpp"
#include "onboard_autonomy/presentation/BoardTypeResolver.hpp"

#include <string>
#include <string_view>

namespace onboard_autonomy::presentation::console {

std::string render_console(
    const application::AppSnapshot& snapshot,
    std::string_view transport_description,
    bool use_color = true,
    const BoardTypeResolver* board_type_resolver = nullptr
);

}  // namespace onboard_autonomy::presentation::console
