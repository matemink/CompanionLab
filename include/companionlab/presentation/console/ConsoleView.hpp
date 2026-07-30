#pragma once

#include "companionlab/application/AppSnapshot.hpp"
#include "companionlab/presentation/BoardTypeResolver.hpp"

#include <string>
#include <string_view>

namespace companionlab::presentation::console {

std::string render_console(
    const application::AppSnapshot& snapshot,
    std::string_view transport_description,
    bool use_color = true,
    const BoardTypeResolver* board_type_resolver = nullptr
);

}  // namespace companionlab::presentation::console
