#include "onboard_autonomy/application/AppSnapshot.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace onboard_autonomy::application {
namespace {

std::string_view camera_phase_name(
    const ports::CameraSourcePhase phase
) {
    switch (phase) {
        case ports::CameraSourcePhase::starting:
            return "starting";
        case ports::CameraSourcePhase::streaming:
            return "streaming";
        case ports::CameraSourcePhase::stopped:
            return "stopped";
        case ports::CameraSourcePhase::failed:
            return "failed";
    }
    return "failed";
}

std::string json_escape(const std::string_view value) {
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
            case '\\':
                output << "\\\\";
                break;
            case '"':
                output << "\\\"";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                output << character;
                break;
        }
    }
    return output.str();
}

void write_optional_decimal(
    std::ostringstream& output,
    const std::optional<double>& value
) {
    if (value.has_value()) {
        output << std::fixed << std::setprecision(3) << *value;
    } else {
        output << "null";
    }
}

}  // namespace

std::string AppSnapshot::to_json() const {
    std::string vehicle_json = vehicle.to_json();
    if (!vehicle_json.empty() && vehicle_json.back() == '}') {
        vehicle_json.pop_back();
    }

    std::ostringstream output;
    output << vehicle_json << ",\"camera\":";
    if (!camera.has_value()) {
        output << "null}";
        return output.str();
    }

    output << '{';
    output << "\"phase\":\""
           << camera_phase_name(camera->phase) << '"';
    output << ",\"source\":\""
           << json_escape(camera->source) << '"';
    output << ",\"error\":\""
           << json_escape(camera->error) << '"';
    output << ",\"width\":" << camera->width;
    output << ",\"height\":" << camera->height;
    output << ",\"received_frames\":"
           << camera->received_frames;
    output << ",\"dropped_before_processing\":"
           << camera->dropped_before_processing;
    output << ",\"frames_with_capture_timestamp\":"
           << camera->frames_with_capture_timestamp;
    output << ",\"measured_fps\":";
    write_optional_decimal(output, camera->measured_fps);
    output << ",\"latest_latency_ms\":";
    write_optional_decimal(output, camera->latest_latency_ms);
    output << ",\"average_latency_ms\":";
    write_optional_decimal(output, camera->average_latency_ms);
    output << ",\"maximum_latency_ms\":";
    write_optional_decimal(output, camera->maximum_latency_ms);
    output << ",\"latest_frame_age_ms\":";
    write_optional_decimal(output, camera->latest_frame_age_ms);
    output << '}';
    output << ",\"vision\":";
    if (!vision.has_value()) {
        output << "null}";
        return output.str();
    }

    output << '{';
    output << "\"detector\":\""
           << json_escape(vision->detector) << '"';
    output << ",\"processed_frames\":"
           << vision->processed_frames;
    output << ",\"frames_with_targets\":"
           << vision->frames_with_targets;
    output << ",\"total_targets\":"
           << vision->total_targets;
    output << ",\"latest_processing_ms\":";
    write_optional_decimal(output, vision->latest_processing_ms);
    output << ",\"average_processing_ms\":";
    write_optional_decimal(output, vision->average_processing_ms);
    output << ",\"maximum_processing_ms\":";
    write_optional_decimal(output, vision->maximum_processing_ms);
    output << ",\"last_detection_age_ms\":";
    write_optional_decimal(output, vision->last_detection_age_ms);
    output << ",\"targets\":[";
    for (std::size_t index = 0;
         index < vision->latest_targets.size();
         ++index) {
        if (index > 0U) {
            output << ',';
        }
        const auto& target = vision->latest_targets[index];
        output << '{';
        output << "\"id\":" << target.id;
        output << ",\"family\":\""
               << json_escape(target.family) << '"';
        output << ",\"center_x_px\":" << std::fixed
               << std::setprecision(2) << target.center.x_px;
        output << ",\"center_y_px\":" << target.center.y_px;
        output << ",\"corrected_bits\":"
               << target.corrected_bits;
        output << ",\"decision_margin\":"
               << target.decision_margin;
        output << ",\"pose\":";
        if (!target.pose.has_value()) {
            output << "null";
        } else {
            output << std::setprecision(4);
            output << "{\"frame\":\"camera_optical\"";
            output << ",\"right_m\":"
                   << target.pose->position.right_m;
            output << ",\"down_m\":"
                   << target.pose->position.down_m;
            output << ",\"forward_m\":"
                   << target.pose->position.forward_m;
            output << ",\"object_space_error\":"
                   << target.pose->object_space_error;
            output << ",\"rotation_tag_to_camera\":[";
            for (std::size_t rotation_index = 0;
                 rotation_index <
                     target.pose->rotation_tag_to_camera.size();
                 ++rotation_index) {
                if (rotation_index > 0U) {
                    output << ',';
                }
                output << target.pose
                              ->rotation_tag_to_camera[rotation_index];
            }
            output << "]}";
        }
        output << '}';
    }
    output << "]}}";
    return output.str();
}

}  // namespace onboard_autonomy::application
