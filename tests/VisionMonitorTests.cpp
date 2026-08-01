#include "TestCases.hpp"

#include "onboard_autonomy/adapters/vision/AprilTagTargetDetector.hpp"
#include "onboard_autonomy/application/AppSnapshot.hpp"
#include "onboard_autonomy/application/VisionMonitor.hpp"

#include <apriltag.h>
#include <common/image_u8.h>
#include <tagStandard41h12.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class FakeTargetDetector final
    : public onboard_autonomy::application::ports::TargetDetector {
public:
    [[nodiscard]] onboard_autonomy::domain::TargetDetectionBatch detect(
        const onboard_autonomy::application::ports::CameraFrame& frame
    ) override {
        std::vector<onboard_autonomy::domain::TargetObservation> targets;
        if (frame.sequence == 1U) {
            targets.push_back(
                {
                    .id = 7,
                    .family = "fake41h12",
                    .center = {.x_px = 100.0, .y_px = 80.0},
                    .corners = {},
                    .corrected_bits = 0,
                    .decision_margin = 90.0,
                }
            );
        }
        return {
            .frame_sequence = frame.sequence,
            .captured_at = frame.captured_at,
            .detected_at = frame.received_at,
            .processing_time =
                std::chrono::microseconds(
                    frame.sequence == 1U ? 2000 : 4000
                ),
            .targets = std::move(targets),
        };
    }

    [[nodiscard]] std::string description() const override {
        return "fake detector";
    }
};

onboard_autonomy::application::ports::CameraFrame empty_frame(
    const std::uint64_t sequence
) {
    return {
        .sequence = sequence,
        .width = 320,
        .height = 240,
        .yuv420 =
            std::vector<std::uint8_t>(320U * 240U * 3U / 2U),
        .captured_at = std::nullopt,
        .received_at = std::chrono::system_clock::now(),
    };
}

void vision_monitor_tracks_processing_and_detections() {
    using namespace std::chrono_literals;

    FakeTargetDetector detector;
    onboard_autonomy::application::VisionMonitor monitor{detector};
    const onboard_autonomy::domain::TimePoint start{};

    monitor.process(empty_frame(1), start);
    auto snapshot = monitor.snapshot(start);
    require(
        snapshot.processed_frames == 1U &&
            snapshot.frames_with_targets == 1U &&
            snapshot.total_targets == 1U &&
            snapshot.latest_targets.size() == 1U &&
            snapshot.latest_targets.front().id == 7,
        "vision monitor must expose the detected target"
    );

    monitor.process(empty_frame(2), start + 10ms);
    snapshot = monitor.snapshot(start + 25ms);
    require(
        snapshot.processed_frames == 2U &&
            snapshot.frames_with_targets == 1U &&
            snapshot.total_targets == 1U &&
            snapshot.latest_targets.empty(),
        "a missing target must not look like a current detection"
    );
    require(
        snapshot.latest_processing_ms.has_value() &&
            std::abs(*snapshot.latest_processing_ms - 4.0) < 0.001 &&
            snapshot.average_processing_ms.has_value() &&
            std::abs(*snapshot.average_processing_ms - 3.0) < 0.001 &&
            snapshot.maximum_processing_ms.has_value() &&
            std::abs(*snapshot.maximum_processing_ms - 4.0) < 0.001,
        "vision monitor must calculate processing latency"
    );
    require(
        snapshot.last_detection_age_ms.has_value() &&
            *snapshot.last_detection_age_ms > 24.9,
        "vision monitor must retain the age of the last detection"
    );
}

void real_apriltag_adapter_detects_generated_id_zero() {
    constexpr std::uint32_t frame_width = 320;
    constexpr std::uint32_t frame_height = 240;
    constexpr std::uint32_t scale = 18;

    apriltag_family_t* family = tagStandard41h12_create();
    require(family != nullptr, "test AprilTag family must be created");
    image_u8_t* tag = apriltag_to_image(family, 0);
    if (tag == nullptr) {
        tagStandard41h12_destroy(family);
        throw std::runtime_error("test AprilTag image must be created");
    }

    auto frame = empty_frame(1);
    std::fill(
        frame.yuv420.begin(),
        frame.yuv420.begin() +
            static_cast<std::ptrdiff_t>(
                frame_width * frame_height
            ),
        static_cast<std::uint8_t>(255)
    );
    std::fill(
        frame.yuv420.begin() +
            static_cast<std::ptrdiff_t>(
                frame_width * frame_height
            ),
        frame.yuv420.end(),
        static_cast<std::uint8_t>(128)
    );

    const auto rendered_width =
        static_cast<std::uint32_t>(tag->width) * scale;
    const auto rendered_height =
        static_cast<std::uint32_t>(tag->height) * scale;
    const auto offset_x = (frame_width - rendered_width) / 2U;
    const auto offset_y = (frame_height - rendered_height) / 2U;
    for (std::uint32_t source_y = 0;
         source_y < static_cast<std::uint32_t>(tag->height);
         ++source_y) {
        for (std::uint32_t source_x = 0;
             source_x < static_cast<std::uint32_t>(tag->width);
             ++source_x) {
            const auto value = tag->buf[
                source_y * static_cast<std::uint32_t>(tag->stride) +
                source_x
            ];
            for (std::uint32_t dy = 0; dy < scale; ++dy) {
                for (std::uint32_t dx = 0; dx < scale; ++dx) {
                    const auto x = offset_x + source_x * scale + dx;
                    const auto y = offset_y + source_y * scale + dy;
                    frame.yuv420[y * frame_width + x] = value;
                }
            }
        }
    }

    image_u8_destroy(tag);
    tagStandard41h12_destroy(family);

    auto detector =
        onboard_autonomy::adapters::vision::
            make_apriltag_target_detector();
    const auto result = detector->detect(frame);
    require(
        result.targets.size() == 1U,
        "real AprilTag adapter must find one generated tag"
    );
    const auto& target = result.targets.front();
    require(
        target.id == 0 &&
            target.family == "tagStandard41h12" &&
            target.corrected_bits == 0,
        "detected tag identity must match the generated family and ID"
    );
    require(
        std::abs(target.center.x_px - 159.5) < 2.0 &&
            std::abs(target.center.y_px - 119.5) < 2.0 &&
            target.decision_margin > 20.0,
        "detected center and quality must match the rendered marker"
    );
}

void vision_snapshot_is_added_to_json() {
    onboard_autonomy::application::AppSnapshot snapshot;
    snapshot.camera = onboard_autonomy::application::CameraSnapshot{};
    snapshot.vision = onboard_autonomy::application::VisionSnapshot{
        .detector = "AprilTag 3 / tagStandard41h12",
        .processed_frames = 10,
        .frames_with_targets = 1,
        .total_targets = 1,
        .latest_processing_ms = 5.5,
        .average_processing_ms = 5.0,
        .maximum_processing_ms = 6.0,
        .last_detection_age_ms = 20.0,
        .latest_targets =
            {
                {
                    .id = 0,
                    .family = "tagStandard41h12",
                    .center = {.x_px = 160.0, .y_px = 120.0},
                    .corners = {},
                    .corrected_bits = 0,
                    .decision_margin = 80.0,
                },
            },
    };

    const auto json = snapshot.to_json();
    require(
        json.find("\"vision\":{") != std::string::npos &&
            json.find("\"processed_frames\":10") !=
                std::string::npos &&
            json.find("\"id\":0") != std::string::npos &&
            json.find("\"center_x_px\":160.00") !=
                std::string::npos,
        "application JSON must expose typed vision results"
    );
}

}  // namespace

void run_vision_monitor_tests() {
    vision_monitor_tracks_processing_and_detections();
    real_apriltag_adapter_detects_generated_id_zero();
    vision_snapshot_is_added_to_json();
}
