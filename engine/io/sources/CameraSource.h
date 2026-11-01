#pragma once
#include "io/ISensorSource.h"
#include "io/ai/PersonTracker.h"
#include <functional>
#include <memory>
namespace mikmap {
struct CameraSourceConfig {
    std::string cameraId;
    int width=640, height=480;
    bool flip=false;
    CamMode mode=CamMode::Off;
    OrtDevice device=OrtDevice::Auto;
    std::string runtimeDir, modelPath;
    float personThr=.5f, jointThr=.3f;
    TrackerParams tracker;
    std::string replayPath;
};
struct CameraStats { float camFps=0,inferFps=0,inferMs=0,latencyMs=0; int people=0; bool onCpu=false; };
class CameraSource final : public ISensorSource {
public:
    // Test seam exercises slow inference and loss without installing an ONNX pack.
    using Inference=std::function<int(const CameraFrame&,PersonPose*,int,std::string&)>;
    explicit CameraSource(CameraSourceConfig, std::unique_ptr<ICameraDevice> dev=nullptr, Inference infer={});
    ~CameraSource() override;
    const char* typeName() const override { return "Camera"; }
    bool start() override;
    void stop() override;
    SourceStatus status() const override;
    std::string lastError() const override;
    // Processed capture preview; compare tCaptureNs with frames() before overlaying points.
    // The two channels publish separately. Single render consumer; pre-reserve RGB to avoid first-copy allocation.
    bool previewLatest(CameraFrame& out);
    CameraStats stats() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
