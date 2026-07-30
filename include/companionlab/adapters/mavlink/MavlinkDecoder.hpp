#pragma once

#include "companionlab/domain/VehicleState.hpp"
#include "companionlab/adapters/mavlink/CommandAck.hpp"

#include <ardupilotmega/mavlink.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace companionlab::adapters::mavlink {

struct MessageObservation {
    std::uint32_t message_id{0};
    std::uint8_t source_system{0};
    std::uint8_t source_component{0};
    std::string_view message_name;
};

class MavlinkDecoder {
public:
    using CommandAckHandler =
        std::function<void(const CommandAck&, domain::TimePoint)>;
    using MessageHandler =
        std::function<void(const MessageObservation&, domain::TimePoint)>;

    explicit MavlinkDecoder(
        domain::VehicleState& state,
        CommandAckHandler command_ack_handler = {},
        MessageHandler message_handler = {}
    );

    void ingest(std::span<const std::uint8_t> bytes, domain::TimePoint now);

private:
    void handle_message(const mavlink_message_t& message, domain::TimePoint now);

    domain::VehicleState& state_;
    CommandAckHandler command_ack_handler_;
    MessageHandler message_handler_;
    mavlink_message_t receive_message_{};
    mavlink_status_t receive_status_{};
};

}  // namespace companionlab::adapters::mavlink
