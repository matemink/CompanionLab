#pragma once

#include "companionlab/application/AppSnapshot.hpp"
#include "companionlab/application/ports/CameraPreviewSink.hpp"
#include "companionlab/application/ports/Transport.hpp"

#include <memory>

namespace companionlab::application {

struct CompanionApplicationOptions {
    ScenarioRunnerConfig scenario_runner;
    bool motion_commands_allowed{false};
    ports::CameraSource* camera_source{nullptr};
    ports::TargetDetector* target_detector{nullptr};
    ports::CameraPreviewSink* camera_preview_sink{nullptr};
};

class CompanionApplication {
public:
    explicit CompanionApplication(
        ports::Transport& transport,
        CompanionApplicationOptions options = {}
    );
    ~CompanionApplication();

    CompanionApplication(const CompanionApplication&) = delete;
    CompanionApplication& operator=(const CompanionApplication&) = delete;
    CompanionApplication(CompanionApplication&&) = delete;
    CompanionApplication& operator=(CompanionApplication&&) = delete;

    void poll(domain::TimePoint now);
    [[nodiscard]] bool trigger_scenario(
        ScenarioId id,
        domain::TimePoint now
    );
    [[nodiscard]] bool request_land(domain::TimePoint now);
    [[nodiscard]] AppSnapshot snapshot(domain::TimePoint now);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace companionlab::application
