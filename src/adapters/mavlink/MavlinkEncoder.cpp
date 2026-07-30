#include "companionlab/adapters/mavlink/MavlinkEncoder.hpp"

#include <ardupilotmega/mavlink.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace companionlab::adapters::mavlink {
namespace {

constexpr std::array<char, 16> kBatteryArmingVoltageParameter{
    'B', 'A', 'T', 'T', '_', 'A', 'R', 'M', '_', 'V', 'O', 'L', 'T',
};

std::vector<std::uint8_t> encode_command_long(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t component_id,
    const std::uint16_t command,
    const std::uint8_t confirmation,
    const std::array<float, 7>& parameters
) {
    mavlink_message_t message{};
    mavlink_msg_command_long_pack(
        vehicle_system_id,
        component_id,
        &message,
        vehicle_system_id,
        0,
        command,
        confirmation,
        parameters[0],
        parameters[1],
        parameters[2],
        parameters[3],
        parameters[4],
        parameters[5],
        parameters[6]
    );

    std::array<std::uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const auto length = mavlink_msg_to_send_buffer(
        buffer.data(),
        &message
    );
    return {buffer.begin(), buffer.begin() + length};
}

}  // namespace

std::vector<std::uint8_t> encode_companion_heartbeat(
    const std::uint8_t system_id,
    const std::uint8_t component_id
) {
    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(
        system_id,
        component_id,
        &message,
        static_cast<std::uint8_t>(MAV_TYPE_ONBOARD_CONTROLLER),
        static_cast<std::uint8_t>(MAV_AUTOPILOT_INVALID),
        0,
        0,
        static_cast<std::uint8_t>(MAV_STATE_ACTIVE)
    );

    std::array<std::uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const auto length = mavlink_msg_to_send_buffer(
        buffer.data(),
        &message
    );
    return {buffer.begin(), buffer.begin() + length};
}

std::vector<std::uint8_t> encode_set_message_interval(
    const std::uint8_t vehicle_system_id,
    const std::uint32_t message_id,
    const std::uint32_t interval_microseconds,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_SET_MESSAGE_INTERVAL,
        confirmation,
        {
            static_cast<float>(message_id),
            static_cast<float>(interval_microseconds),
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
        }
    );
}

std::vector<std::uint8_t> encode_battery_arming_voltage_request(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t component_id
) {
    mavlink_message_t message{};
    mavlink_msg_param_request_read_pack(
        vehicle_system_id,
        component_id,
        &message,
        vehicle_system_id,
        MAV_COMP_ID_AUTOPILOT1,
        kBatteryArmingVoltageParameter.data(),
        -1
    );

    std::array<std::uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const auto length = mavlink_msg_to_send_buffer(
        buffer.data(),
        &message
    );
    return {buffer.begin(), buffer.begin() + length};
}

std::vector<std::uint8_t> encode_autopilot_version_request(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_REQUEST_MESSAGE,
        0,
        {
            static_cast<float>(MAVLINK_MSG_ID_AUTOPILOT_VERSION),
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            1.0F,
        }
    );
}

std::vector<std::uint8_t> encode_set_guided_mode(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_DO_SET_MODE,
        confirmation,
        {
            static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED),
            4.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
        }
    );
}

std::vector<std::uint8_t> encode_arm(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_COMPONENT_ARM_DISARM,
        confirmation,
        {
            1.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
        }
    );
}

std::vector<std::uint8_t> encode_takeoff(
    const std::uint8_t vehicle_system_id,
    const double altitude_m,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_NAV_TAKEOFF,
        confirmation,
        {
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            static_cast<float>(altitude_m),
        }
    );
}

std::vector<std::uint8_t> encode_land(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_NAV_LAND,
        confirmation,
        {
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
        }
    );
}

std::vector<std::uint8_t> encode_return_to_launch(
    const std::uint8_t vehicle_system_id,
    const std::uint8_t confirmation,
    const std::uint8_t component_id
) {
    return encode_command_long(
        vehicle_system_id,
        component_id,
        MAV_CMD_NAV_RETURN_TO_LAUNCH,
        confirmation,
        {
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
            0.0F,
        }
    );
}

std::vector<std::uint8_t> encode_local_position_target(
    const std::uint8_t vehicle_system_id,
    const double north_m,
    const double east_m,
    const double down_m,
    const std::uint8_t component_id
) {
    mavlink_message_t message{};
    mavlink_msg_set_position_target_local_ned_pack(
        vehicle_system_id,
        component_id,
        &message,
        0,
        vehicle_system_id,
        0,
        MAV_FRAME_LOCAL_OFFSET_NED,
        3576,
        static_cast<float>(north_m),
        static_cast<float>(east_m),
        static_cast<float>(down_m),
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F,
        0.0F
    );

    std::array<std::uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const auto length = mavlink_msg_to_send_buffer(
        buffer.data(),
        &message
    );
    return {buffer.begin(), buffer.begin() + length};
}

std::vector<std::uint8_t> encode_landing_target(
    const std::uint8_t vehicle_system_id,
    const std::uint64_t time_usec,
    const double forward_m,
    const double right_m,
    const double down_m,
    const std::uint8_t component_id
) {
    mavlink_message_t message{};
    const std::array<float, 4> orientation{
        1.0F,
        0.0F,
        0.0F,
        0.0F,
    };
    const auto distance = std::sqrt(
        forward_m * forward_m +
        right_m * right_m +
        down_m * down_m
    );

    mavlink_msg_landing_target_pack(
        vehicle_system_id,
        component_id,
        &message,
        time_usec,
        0,
        MAV_FRAME_BODY_FRD,
        0.0F,
        0.0F,
        static_cast<float>(distance),
        0.0F,
        0.0F,
        static_cast<float>(forward_m),
        static_cast<float>(right_m),
        static_cast<float>(down_m),
        orientation.data(),
        LANDING_TARGET_TYPE_VISION_FIDUCIAL,
        1
    );

    std::array<std::uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const auto length = mavlink_msg_to_send_buffer(
        buffer.data(),
        &message
    );
    return {buffer.begin(), buffer.begin() + length};
}

}  // namespace companionlab::adapters::mavlink
