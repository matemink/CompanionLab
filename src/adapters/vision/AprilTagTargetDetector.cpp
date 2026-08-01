#include "onboard_autonomy/adapters/vision/AprilTagTargetDetector.hpp"

#include <apriltag.h>
#include <common/image_types.h>
#include <common/zarray.h>
#include <tagStandard41h12.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace onboard_autonomy::adapters::vision {
namespace {

class DetectionArray {
public:
    explicit DetectionArray(zarray_t* detections)
        : detections_(detections) {}

    ~DetectionArray() {
        if (detections_ != nullptr) {
            apriltag_detections_destroy(detections_);
        }
    }

    DetectionArray(const DetectionArray&) = delete;
    DetectionArray& operator=(const DetectionArray&) = delete;

    [[nodiscard]] zarray_t* get() const {
        return detections_;
    }

private:
    zarray_t* detections_;
};

class AprilTagTargetDetector final
    : public application::ports::TargetDetector {
public:
    explicit AprilTagTargetDetector(
        const AprilTagDetectorConfig& config
    ) {
        if (config.worker_threads == 0U ||
            config.worker_threads >
                static_cast<std::uint32_t>(
                    std::numeric_limits<int>::max()
                ) ||
            config.quad_decimate < 1.0 ||
            config.corrected_bits < 0 ||
            config.corrected_bits > 2) {
            throw std::invalid_argument(
                "invalid AprilTag detector configuration"
            );
        }

        family_ = tagStandard41h12_create();
        detector_ = apriltag_detector_create();
        if (family_ == nullptr || detector_ == nullptr) {
            if (detector_ != nullptr) {
                apriltag_detector_destroy(detector_);
            }
            if (family_ != nullptr) {
                tagStandard41h12_destroy(family_);
            }
            throw std::runtime_error(
                "unable to create AprilTag detector"
            );
        }

        detector_->nthreads =
            static_cast<int>(config.worker_threads);
        detector_->quad_decimate =
            static_cast<float>(config.quad_decimate);
        detector_->quad_sigma = 0.0F;
        detector_->refine_edges = config.refine_edges;
        detector_->debug = false;
        apriltag_detector_add_family_bits(
            detector_,
            family_,
            config.corrected_bits
        );
    }

    ~AprilTagTargetDetector() override {
        if (family_ != nullptr) {
            tagStandard41h12_destroy(family_);
        }
        if (detector_ != nullptr) {
            apriltag_detector_destroy(detector_);
        }
    }

    [[nodiscard]] domain::TargetDetectionBatch detect(
        const application::ports::CameraFrame& frame
    ) override {
        const std::uint64_t luma_bytes =
            static_cast<std::uint64_t>(frame.width) *
            static_cast<std::uint64_t>(frame.height);
        if (frame.width == 0U || frame.height == 0U ||
            luma_bytes > frame.yuv420.size() ||
            frame.width >
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max()
                ) ||
            frame.height >
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max()
                )) {
            throw std::invalid_argument(
                "AprilTag detector requires a complete Y plane"
            );
        }

        image_u8_t image{
            .width = static_cast<std::int32_t>(frame.width),
            .height = static_cast<std::int32_t>(frame.height),
            .stride = static_cast<std::int32_t>(frame.width),
            // AprilTag's C API is mutable, but detection treats the
            // source image as input when quad_sigma is zero.
            .buf = const_cast<std::uint8_t*>(
                frame.yuv420.data()
            ),
        };

        const auto processing_started =
            std::chrono::steady_clock::now();
        DetectionArray detections{
            apriltag_detector_detect(detector_, &image)
        };
        const auto detected_at =
            std::chrono::system_clock::now();
        const auto processing_finished =
            std::chrono::steady_clock::now();
        if (detections.get() == nullptr) {
            throw std::runtime_error(
                "AprilTag detector returned no result array"
            );
        }

        std::vector<domain::TargetObservation> targets;
        const int count = zarray_size(detections.get());
        targets.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index) {
            apriltag_detection_t* detection = nullptr;
            zarray_get(detections.get(), index, &detection);
            if (detection == nullptr) {
                continue;
            }

            std::array<domain::ImagePoint, 4> corners{};
            for (std::size_t corner = 0;
                 corner < corners.size();
                 ++corner) {
                corners[corner] = {
                    .x_px = detection->p[corner][0],
                    .y_px = detection->p[corner][1],
                };
            }
            targets.push_back(
                {
                    .id = detection->id,
                    .family =
                        detection->family != nullptr &&
                                detection->family->name != nullptr
                            ? detection->family->name
                            : "tagStandard41h12",
                    .center =
                        {
                            .x_px = detection->c[0],
                            .y_px = detection->c[1],
                        },
                    .corners = corners,
                    .corrected_bits = detection->hamming,
                    .decision_margin =
                        static_cast<double>(
                            detection->decision_margin
                        ),
                }
            );
        }

        return {
            .frame_sequence = frame.sequence,
            .captured_at = frame.captured_at,
            .detected_at = detected_at,
            .processing_time =
                std::chrono::duration_cast<
                    std::chrono::microseconds
                >(processing_finished - processing_started),
            .targets = std::move(targets),
        };
    }

    [[nodiscard]] std::string description() const override {
        return "AprilTag 3 / tagStandard41h12";
    }

private:
    apriltag_family_t* family_{nullptr};
    apriltag_detector_t* detector_{nullptr};
};

}  // namespace

std::unique_ptr<application::ports::TargetDetector>
make_apriltag_target_detector(AprilTagDetectorConfig config) {
    return std::make_unique<AprilTagTargetDetector>(config);
}

}  // namespace onboard_autonomy::adapters::vision
