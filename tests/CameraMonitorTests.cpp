#include "TestCases.hpp"

#include "onboard_autonomy/adapters/camera/RpicamCameraSource.hpp"
#include "onboard_autonomy/application/AppSnapshot.hpp"
#include "onboard_autonomy/application/CameraMonitor.hpp"

#include <chrono>
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

class FakeCameraSource final
    : public onboard_autonomy::application::ports::CameraSource {
public:
    void emit(
        onboard_autonomy::application::ports::CameraFrame frame
    ) {
        latest_ = std::move(frame);
        ++status_.produced_frames;
        status_.phase =
            onboard_autonomy::application::ports::
                CameraSourcePhase::streaming;
    }

    [[nodiscard]] std::optional<
        onboard_autonomy::application::ports::CameraFrame
    > take_latest_frame() override {
        auto frame = std::move(latest_);
        latest_.reset();
        return frame;
    }

    [[nodiscard]]
    onboard_autonomy::application::ports::CameraSourceStatus
    status() const override {
        return status_;
    }

private:
    onboard_autonomy::application::ports::CameraSourceStatus status_{
        .description = "fake camera",
        .error = "",
    };
    std::optional<
        onboard_autonomy::application::ports::CameraFrame
    > latest_;
};

class FakeTargetDetector final
    : public onboard_autonomy::application::ports::TargetDetector {
public:
    [[nodiscard]] onboard_autonomy::domain::TargetDetectionBatch detect(
        const onboard_autonomy::application::ports::CameraFrame& input
    ) override {
        return {
            .frame_sequence = input.sequence,
            .captured_at = input.captured_at,
            .detected_at = input.received_at,
            .processing_time = std::chrono::microseconds{500},
            .targets =
                {
                    {
                        .id = 12,
                        .family = "fake",
                        .center = {.x_px = 320.0, .y_px = 240.0},
                        .corners = {},
                        .corrected_bits = 0,
                        .decision_margin = 50.0,
                    },
                },
        };
    }

    [[nodiscard]] std::string description() const override {
        return "fake target detector";
    }
};

class FakeCameraPreviewSink final
    : public onboard_autonomy::application::ports::CameraPreviewSink {
public:
    void publish(
        const onboard_autonomy::application::ports::CameraFrame& input,
        const std::span<
            const onboard_autonomy::domain::TargetObservation
        > input_targets
    ) override {
        sequence = input.sequence;
        luma_size =
            static_cast<std::size_t>(input.width) * input.height;
        targets.assign(input_targets.begin(), input_targets.end());
    }

    [[nodiscard]] std::string description() const override {
        return "fake camera preview";
    }

    std::uint64_t sequence{0};
    std::size_t luma_size{0};
    std::vector<onboard_autonomy::domain::TargetObservation> targets;
};

onboard_autonomy::application::ports::CameraFrame frame(
    const std::uint64_t sequence,
    const std::chrono::system_clock::time_point captured_at,
    const double latency_ms
) {
    return {
        .sequence = sequence,
        .width = 640,
        .height = 480,
        .yuv420 = std::vector<std::uint8_t>(640U * 480U * 3U / 2U),
        .captured_at = captured_at,
        .received_at =
            captured_at +
            std::chrono::duration_cast<
                std::chrono::system_clock::duration
            >(std::chrono::duration<double, std::milli>(latency_ms)),
    };
}

void monitor_calculates_frame_rate_latency_and_gaps() {
    using namespace std::chrono_literals;

    FakeCameraSource source;
    onboard_autonomy::application::CameraMonitor monitor{source};
    const auto wall_start =
        std::chrono::system_clock::time_point{1000s};
    const onboard_autonomy::domain::TimePoint app_start{};

    source.emit(frame(1, wall_start, 20.0));
    monitor.poll(app_start);
    source.emit(frame(2, wall_start + 40ms, 30.0));
    monitor.poll(app_start + 40ms);
    source.emit(frame(4, wall_start + 80ms, 40.0));
    monitor.poll(app_start + 80ms);

    const auto snapshot = monitor.snapshot(app_start + 90ms);
    require(
        snapshot.phase ==
            onboard_autonomy::application::ports::
                CameraSourcePhase::streaming,
        "camera monitor must expose streaming state"
    );
    require(
        snapshot.received_frames == 3U &&
            snapshot.dropped_before_processing == 1U,
        "camera monitor must count frames and sequence gaps"
    );
    require(
        snapshot.measured_fps.has_value() &&
            *snapshot.measured_fps > 24.9 &&
            *snapshot.measured_fps < 25.1,
        "camera monitor must measure FPS from capture timestamps"
    );
    require(
        snapshot.latest_latency_ms.has_value() &&
            *snapshot.latest_latency_ms > 39.9 &&
            snapshot.average_latency_ms.has_value() &&
            *snapshot.average_latency_ms > 29.9 &&
            *snapshot.average_latency_ms < 30.1 &&
            snapshot.maximum_latency_ms.has_value() &&
            *snapshot.maximum_latency_ms > 39.9,
        "camera monitor must calculate latest, average and max latency"
    );
    require(
        snapshot.latest_frame_age_ms.has_value() &&
            *snapshot.latest_frame_age_ms > 9.9,
        "camera monitor must expose frame age on the application clock"
    );
}

void metadata_parser_accepts_only_frame_wall_clock() {
    const auto parsed =
        onboard_autonomy::adapters::camera::
            parse_rpicam_frame_wall_clock_ns(
                "    \"FrameWallClock\": 1785440325818936064,"
            );
    require(
        parsed.has_value() &&
            *parsed == 1785440325818936064LL,
        "rpicam parser must read the nanosecond wall-clock timestamp"
    );
    require(
        !onboard_autonomy::adapters::camera::
             parse_rpicam_frame_wall_clock_ns(
                 "    \"SensorTimestamp\": 413528965000"
             ).has_value(),
        "rpicam parser must not confuse monotonic and wall clocks"
    );
}

void monitor_forwards_the_processed_frame_to_preview() {
    FakeCameraSource source;
    FakeTargetDetector detector;
    FakeCameraPreviewSink preview;
    onboard_autonomy::application::CameraMonitor monitor{
        source,
        &detector,
        &preview,
    };

    source.emit(
        frame(
            21,
            std::chrono::system_clock::time_point{},
            10.0
        )
    );
    monitor.poll(onboard_autonomy::domain::TimePoint{});

    require(
        preview.sequence == 21U &&
            preview.luma_size == 640U * 480U &&
            preview.targets.size() == 1U &&
            preview.targets.front().id == 12,
        "camera preview must receive the frame and its detections"
    );
}

void app_snapshot_json_preserves_vehicle_and_adds_camera() {
    onboard_autonomy::application::AppSnapshot snapshot;
    snapshot.camera = onboard_autonomy::application::CameraSnapshot{
        .phase =
            onboard_autonomy::application::ports::
                CameraSourcePhase::streaming,
        .source = "Camera Module 3 \"Wide\"",
        .error = "",
        .width = 640,
        .height = 480,
        .received_frames = 5,
        .dropped_before_processing = 0,
        .frames_with_capture_timestamp = 5,
        .measured_fps = std::nullopt,
        .latest_latency_ms = 21.5,
        .average_latency_ms = std::nullopt,
        .maximum_latency_ms = std::nullopt,
        .latest_frame_age_ms = std::nullopt,
    };

    const std::string json = snapshot.to_json();
    require(
        json.find("\"connected\":false") != std::string::npos &&
            json.find("\"camera\":{") != std::string::npos &&
            json.find("\"phase\":\"streaming\"") !=
                std::string::npos &&
            json.find("Camera Module 3 \\\"Wide\\\"") !=
                std::string::npos &&
            json.find("\"latest_latency_ms\":21.500") !=
                std::string::npos,
        "application JSON must remain backward compatible and add camera"
    );
}

}  // namespace

void run_camera_monitor_tests() {
    monitor_calculates_frame_rate_latency_and_gaps();
    metadata_parser_accepts_only_frame_wall_clock();
    monitor_forwards_the_processed_frame_to_preview();
    app_snapshot_json_preserves_vehicle_and_adds_camera();
}
