# Task 6 — PersonTracker

Implemented bounded 30-slot, allocation-free tracker: keypoint bounding-box IoU greedy assignment with hip-centre distance tie-break, constant-velocity prediction for crossing paths, lowest-free-slot reuse, retained unseen poses, confidence-priority admission, maxPeople runtime reduction, and optional light/medium smoothing. Smoothing off copies raw poses exactly. Invalid time/output is a no-op; backward time clamps; invalid parameters clamp/default. No existing callers or manual CMake registration needed (source/test globs).

RED: added four tracker cases first; Release build failed C1083 for missing io/ai/PersonTracker.h.
GREEN: Release /W4 /permissive- /fp:precise build succeeded without warnings. Direct build/mikmap_tests.exe full suite: 569 passed, 0 failed, 11053 assertions. The first full run found a test fixture gap (survivor not observed before timeout); fixture now observes it at .8 before checking timeout at 1.0.

Tests cover 10 crossing frames with alternating detection ordering; .4s disappearance, .6s expiration and slot reuse; strongest 10 of 15; 1..30 bounds; lowering maxPeople drops high slots; exact raw coordinates; null/invalid outputs, nonfinite/backward time, invalid params, invalid score, and smoothing.

Optional camera, ORT integration, and RTMO gray-frame model tests explicitly reported skipped because MIKMAP_TEST_CAMERA, MIKMAP_TEST_ORT_DIR, and MIKMAP_TEST_POSE_MODEL are unset. No app build or real-camera validation performed. Greedy motion prediction cannot guarantee identity during arbitrary ambiguous occlusion or abrupt motion; hardware behavior remains to validate.
