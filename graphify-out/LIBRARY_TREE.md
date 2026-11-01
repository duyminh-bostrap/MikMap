# MikMap — cây thư viện cho agent

Phạm vi: checkout gốc hiện tại; bỏ build, worktree, binary và media bị ignore.
Đọc README.md, features.md và code thật để phân biệt tính năng đã có với kiến trúc đích.

```text
Mikmap/
├── .claude/
│   ├── CLAUDE.md
│   ├── agents/
│   │   ├── researcher.md
│   │   └── reviewer.md
│   ├── hooks/
│   │   ├── pre-commit.sh
│   │   └── pre-push.sh
│   ├── memory/
│   │   └── README.md
│   ├── rules/
│   │   ├── design.md
│   │   ├── tech-defaults.md
│   │   └── workflow.md
│   ├── settings.json
│   └── skills/
│       └── run-security-scan.sh
├── .github/
│   └── workflows/
│       └── release.yml
├── .vscode/
│   ├── c_cpp_properties.json
│   ├── launch.json
│   └── tasks.json
├── AGENTS.md
├── CMakeLists.txt
├── README.md
├── REQUIREMENTS.md
├── architecture.md
├── bin/
│   └── data/
│       └── fonts/
│           ├── Inter-OFL.txt
│           ├── LICENSES.txt
│           └── lucide-version.txt
├── cmake/
│   └── configure.cmake
├── docs/
│   └── superpowers/
│       ├── plans/
│       │   ├── 2026-10-27-sensor-skeleton-hokuyo.md
│       │   ├── 2026-10-29-mapping-sensor-unified-s1.md
│       │   ├── 2026-10-30-camera-body-a.md
│       │   └── 2026-10-30-sensor-packs-p.md
│       └── specs/
│           ├── 2026-10-29-mapping-sensor-unified-design.md
│           ├── 2026-10-30-ai-body-tracking-a-design.md
│           └── 2026-10-30-sensor-packs-design.md
├── engine/
│   ├── core/
│   │   ├── calib/
│   │   │   ├── CalibrationProfile.cpp
│   │   │   ├── CalibrationProfile.h
│   │   │   ├── LidarPlacement.cpp
│   │   │   ├── LidarPlacement.h
│   │   │   ├── SensorMapper.cpp
│   │   │   ├── SensorMapper.h
│   │   │   ├── SensorRouting.cpp
│   │   │   └── SensorRouting.h
│   │   ├── filter/
│   │   │   ├── OneEuroFilter.cpp
│   │   │   ├── OneEuroFilter.h
│   │   │   ├── PointTracker.cpp
│   │   │   └── PointTracker.h
│   │   ├── math/
│   │   │   ├── BilinearInverse.cpp
│   │   │   ├── BilinearInverse.h
│   │   │   ├── EdgeBlend.cpp
│   │   │   ├── EdgeBlend.h
│   │   │   ├── Homography.cpp
│   │   │   ├── Homography.h
│   │   │   ├── LinearSolver.cpp
│   │   │   ├── LinearSolver.h
│   │   │   ├── Mat3.cpp
│   │   │   ├── Mat3.h
│   │   │   ├── Snapping.cpp
│   │   │   ├── Snapping.h
│   │   │   ├── Triangulate.cpp
│   │   │   ├── Triangulate.h
│   │   │   └── Vec2.h
│   │   ├── model/
│   │   │   ├── BezierMask.cpp
│   │   │   ├── BezierMask.h
│   │   │   ├── BlendMode.cpp
│   │   │   ├── BlendMode.h
│   │   │   ├── Clip.h
│   │   │   ├── Composition.cpp
│   │   │   ├── Composition.h
│   │   │   ├── Deck.cpp
│   │   │   ├── Deck.h
│   │   │   ├── Generators.h
│   │   │   ├── IWarp.h
│   │   │   ├── Layer.h
│   │   │   ├── ProjectIO.cpp
│   │   │   ├── ProjectIO.h
│   │   │   ├── Screen.cpp
│   │   │   ├── Screen.h
│   │   │   ├── Slice.cpp
│   │   │   ├── Slice.h
│   │   │   ├── Transform2D.cpp
│   │   │   ├── Transform2D.h
│   │   │   ├── Transport.cpp
│   │   │   ├── Transport.h
│   │   │   ├── TriggerZone.cpp
│   │   │   ├── TriggerZone.h
│   │   │   ├── WarpBezier.cpp
│   │   │   ├── WarpBezier.h
│   │   │   ├── WarpCornerPin.cpp
│   │   │   ├── WarpCornerPin.h
│   │   │   ├── WarpMesh.cpp
│   │   │   └── WarpMesh.h
│   │   └── util/
│   │       ├── AppSettings.cpp
│   │       ├── AppSettings.h
│   │       ├── Clock.h
│   │       ├── Ed25519.cpp
│   │       ├── Ed25519.h
│   │       ├── Json.cpp
│   │       ├── Json.h
│   │       ├── Sha2.cpp
│   │       └── Sha2.h
│   ├── i18n/
│   │   ├── Localization.cpp
│   │   └── Localization.h
│   └── io/
│       ├── ISensorSource.h
│       ├── LaserScan.h
│       ├── LidarBlobs.cpp
│       ├── LidarBlobs.h
│       ├── SensorFrame.h
│       ├── SensorLog.cpp
│       ├── SensorLog.h
│       ├── SpscRingBuffer.h
│       ├── TripleBuffer.h
│       ├── ai/
│       │   ├── OrtRuntime.cpp
│       │   ├── OrtRuntime.h
│       │   ├── PersonTracker.cpp
│       │   ├── PersonTracker.h
│       │   ├── PoseModel.cpp
│       │   └── PoseModel.h
│       ├── camera/
│       │   ├── CameraDevice.cpp
│       │   ├── CameraDevice.h
│       │   ├── MfCamera.cpp
│       │   ├── MfCamera.h
│       │   └── MfCameraConvert.h
│       ├── net/
│       │   ├── NetScan.cpp
│       │   ├── NetScan.h
│       │   ├── Socket.h
│       │   ├── TouchSender.cpp
│       │   └── TouchSender.h
│       ├── packs/
│       │   ├── HttpGet.cpp
│       │   ├── HttpGet.h
│       │   ├── MikPack.cpp
│       │   ├── MikPack.h
│       │   ├── PackInstaller.cpp
│       │   ├── PackInstaller.h
│       │   ├── PackManifest.cpp
│       │   ├── PackManifest.h
│       │   ├── PackStore.cpp
│       │   └── PackStore.h
│       ├── plugin/
│       │   ├── PluginLoader.cpp
│       │   ├── PluginLoader.h
│       │   └── mikmap_sensor_plugin.h
│       ├── proto/
│       │   ├── OscMessage.cpp
│       │   ├── OscMessage.h
│       │   ├── Scip2.cpp
│       │   ├── Scip2.h
│       │   ├── SerialProtocol.cpp
│       │   ├── SerialProtocol.h
│       │   ├── TuioDecoder.cpp
│       │   └── TuioDecoder.h
│       └── sources/
│           ├── CameraSource.cpp
│           ├── CameraSource.h
│           ├── HokuyoSource.cpp
│           ├── HokuyoSource.h
│           ├── MockSource.cpp
│           ├── MockSource.h
│           ├── OscSource.cpp
│           ├── OscSource.h
│           ├── PluginSource.cpp
│           ├── PluginSource.h
│           ├── ScanSource.cpp
│           └── ScanSource.h
├── features.md
├── sampleUI/
│   └── mikmap_ui.tsx
├── src/
│   ├── CMakeLists.txt
│   ├── app.h
│   ├── assets/
│   │   ├── fonts/
│   │   │   ├── OFL.txt
│   │   │   ├── README.md
│   │   │   └── fonts.json
│   │   └── mikmap-mark.png
│   ├── build.ps1
│   ├── calib.cpp
│   ├── clipart.cpp
│   ├── deck.cpp
│   ├── main.cpp
│   ├── mapping.cpp
│   ├── output.cpp
│   ├── packs.cpp
│   ├── project.cpp
│   ├── sensor.cpp
│   ├── settings.cpp
│   ├── sources.cpp
│   ├── stage.cpp
│   ├── third_party/
│   │   └── stb/
│   │       ├── stb_image.h
│   │       └── stb_image_write.h
│   ├── ui.cpp
│   └── ui.h
├── store/
│   ├── docs/
│   │   ├── 01-overview.md
│   │   ├── 02-composition.md
│   │   ├── 03-advanced-mapping.md
│   │   ├── 04-sensor-io.md
│   │   ├── 05-settings.md
│   │   ├── 06-keyboard-shortcuts.md
│   │   ├── 07-system-requirements.md
│   │   ├── 08-troubleshooting.md
│   │   ├── 09-data-and-privacy.md
│   │   └── 11-release-notes.md
│   ├── images.md
│   ├── license.md
│   ├── preview/
│   │   └── cover.png
│   ├── product.md
│   ├── releases.md
│   └── tutorial/
│       ├── 01-install-and-first-run.md
│       ├── 02-trigger-clips.md
│       ├── 03-add-images.md
│       ├── 04-map-a-slice.md
│       ├── 05-output-to-projector.md
│       └── 06-save-and-open-projects.md
├── tests/
│   ├── FakeHttpServer.h
│   ├── FakeScipServer.h
│   ├── TestHarness.h
│   ├── plugins/
│   │   └── fake_scan_plugin.c
│   ├── test_appsettings.cpp
│   ├── test_beziermask.cpp
│   ├── test_calibration.cpp
│   ├── test_camera.cpp
│   ├── test_camerasource.cpp
│   ├── test_composition.cpp
│   ├── test_ed25519.cpp
│   ├── test_edgeblend.cpp
│   ├── test_filter.cpp
│   ├── test_hokuyo.cpp
│   ├── test_homography.cpp
│   ├── test_httpget.cpp
│   ├── test_io.cpp
│   ├── test_lidarblobs.cpp
│   ├── test_lidarplacement.cpp
│   ├── test_localization.cpp
│   ├── test_main.cpp
│   ├── test_mat3.cpp
│   ├── test_mfcamera.cpp
│   ├── test_mikpack.cpp
│   ├── test_netscan.cpp
│   ├── test_ortruntime.cpp
│   ├── test_osc.cpp
│   ├── test_packinstaller.cpp
│   ├── test_packmanifest.cpp
│   ├── test_packstore.cpp
│   ├── test_persontracker.cpp
│   ├── test_plugin.cpp
│   ├── test_posemodel.cpp
│   ├── test_projectio.cpp
│   ├── test_scip2.cpp
│   ├── test_screen.cpp
│   ├── test_sensorlog.cpp
│   ├── test_sensorrouting.cpp
│   ├── test_serialproto.cpp
│   ├── test_sha2.cpp
│   ├── test_slicesource.cpp
│   ├── test_snapping.cpp
│   ├── test_touchsender.cpp
│   ├── test_transport.cpp
│   ├── test_triangulate.cpp
│   ├── test_triggerzone.cpp
│   ├── test_tuio.cpp
│   ├── test_warp.cpp
│   └── test_warpbezier.cpp
├── third_party/
│   └── onnxruntime/
│       ├── README.md
│       └── onnxruntime_c_api.h
├── tools/
│   ├── packs/
│   │   ├── examples/
│   │   │   ├── README.md
│   │   │   ├── onnx-runtime-cuda/
│   │   │   │   └── pack.json
│   │   │   └── pose-rtmo/
│   │   │       └── pack.json
│   │   └── packtool.cpp
│   └── pose_bridge/
│       ├── README.md
│       ├── pose_bridge.py
│       ├── requirements.txt
│       └── test_pose_bridge.py
├── ux-current.md
└── ux-feedback.md
```

## Điểm vào theo nhiệm vụ

| Nhiệm vụ | Đường dẫn |
|---|---|
| App/UI hiện tại | `src/main.cpp`, `src/app.h` |
| Deck / column / layer | `src/deck.cpp` |
| Mapping và hình học slice | `src/mapping.cpp` |
| Sensor UI và nguồn đang chạy | `src/sensor.cpp`, `src/sources.cpp` |
| Output / texture / generator | `src/output.cpp`, `src/clipart.cpp` |
| Project và undo | `src/project.cpp` |
| Toán và mô hình không GL | `engine/core/` |
| Driver, giao thức, biên thread | `engine/io/` |
| Chuỗi ngôn ngữ | `engine/i18n/` |
| Sidecar MediaPipe | `tools/pose_bridge/` |
| Test engine | `tests/`, `CMakeLists.txt` |
| Build app | `src/CMakeLists.txt` |

Các đường dẫn trên là điểm bắt đầu, không khẳng định toàn bộ chức năng đã được triển khai.

## Cây target CMake (đã đối chiếu)

```text
Root CMakeLists.txt
├── mikmap_core                 engine/core; STL
├── mikmap_io → mikmap_core     engine/io; Threads + thư viện OS
├── mikmap_i18n → mikmap_core   engine/i18n
├── mikmap_tests → core + io + i18n
├── mikmap_packtool → io
└── fake_scan_plugin / v999     fixture ABI plugin

src/CMakeLists.txt (project riêng)
└── mikmap
    ├── src/*.cpp
    ├── engine/core + engine/io được biên dịch vào target khi WITH_ENGINE bật
    ├── Dear ImGui
    └── GLFW + OpenGL + Threads + thư viện OS
```

Nguồn: `CMakeLists.txt:70,87,89,111,116,142,143,153,154`; `src/CMakeLists.txt:33,34,72,92`.
App biên dịch nguồn engine vào executable; không diễn giải thành quan hệ link target riêng nếu CMake không khai báo như vậy.
