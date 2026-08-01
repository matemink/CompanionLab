#pragma once

#include "onboard_autonomy/application/ports/Transport.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace onboard_autonomy::adapters::transport {

std::unique_ptr<application::ports::Transport> make_udp_transport(
    const std::string& bind_address,
    std::uint16_t port
);

std::unique_ptr<application::ports::Transport> make_serial_transport(
    const std::string& device,
    std::uint32_t baud_rate
);

}  // namespace onboard_autonomy::adapters::transport
