#pragma once

#include "companionlab/application/VisionMonitor.hpp"
#include "companionlab/application/ports/CameraPreviewSink.hpp"
#include "companionlab/application/ports/CameraSource.hpp"
#include "companionlab/domain/VehicleState.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace companionlab::application {

struct CameraSnapshot {
    ports::CameraSourcePhase phase{
        ports::CameraSourcePhase::starting
    };
    std::string source;
    std::string error;
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t received_frames{0};
    std::uint64_t dropped_before_processing{0};
    std::uint64_t frames_with_capture_timestamp{0};
    std::optional<double> measured_fps;
    std::optional<double> latest_latency_ms;
    std::optional<double> average_latency_ms;
    std::optional<double> maximum_latency_ms;
    std::optional<double> latest_frame_age_ms;
};

class CameraMonitor {
public:
    explicit CameraMonitor(
        ports::CameraSource& source,
        ports::TargetDetector* target_detector = nullptr,
        ports::CameraPreviewSink* preview_sink = nullptr
    );
    ~CameraMonitor();

    CameraMonitor(const CameraMonitor&) = delete;
    CameraMonitor& operator=(const CameraMonitor&) = delete;
    CameraMonitor(CameraMonitor&&) noexcept;
    CameraMonitor& operator=(CameraMonitor&&) noexcept;

    void poll(domain::TimePoint now);
    [[nodiscard]] CameraSnapshot snapshot(domain::TimePoint now) const;
    [[nodiscard]] std::optional<VisionSnapshot> vision_snapshot(
        domain::TimePoint now
    ) const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace companionlab::application
