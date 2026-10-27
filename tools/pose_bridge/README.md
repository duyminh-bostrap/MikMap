# pose_bridge

MediaPipe Pose sidecar for MikMap's **Camera skeleton** sensor: camera -> MediaPipe Pose
Landmarker -> one OSC bundle per frame over UDP. It runs as a separate process, so a crash
or a slow model cannot take the show down.

## Setup

```bash
python -m venv tools/pose_bridge/.venv
tools/pose_bridge/.venv/Scripts/activate        # Linux/macOS: source tools/pose_bridge/.venv/bin/activate
pip install -r tools/pose_bridge/requirements.txt
python tools/pose_bridge/pose_bridge.py          # first run downloads the .task model next to the script
```

In MikMap enable the "Camera skeleton" device on the Sensor page (UDP port 9000 by default).

## Wire protocol

One OSC bundle per camera frame, sent to `--host:--port`:

| Message | Meaning |
|---|---|
| `/mikmap/meta i:frame f:latency_ms f:fps` | first in the bundle |
| `/mikmap/touch i:id f:x f:y f:z` | one per visible joint; x, y = raw normalised image coords, z = landmark z |
| `/mikmap/touch/up i:id` | for each id sent last frame and missing now |

`id = slot * 100 + landmark` (landmark 0..32 as in MediaPipe Pose). Slots are kept per
person across frames (nearest hip-centre within 0.2); a new person takes the lowest free
slot. Joints with visibility below `--min-visibility` are dropped.

## Flags

| Flag | Default | |
|---|---|---|
| `--camera` | 0 | camera index (DirectShow on Windows) |
| `--width` / `--height` | 640 / 480 | capture size |
| `--model` | lite | `lite`, `full` or `heavy` |
| `--num-poses` | 1 | people tracked |
| `--joints` | all | `all` (33) or `key` (0, 11, 12, 15, 16, 23, 24, 27, 28) |
| `--min-visibility` | 0.5 | drop joints below this |
| `--host` / `--port` | 127.0.0.1 / 9000 | OSC destination |

`num-poses x joints` must be at most 64 (sensor frame capacity), otherwise exit code 2.
Exit code 3 means the camera was lost (30 failed reads in a row). A line with fps and
latency is printed once per second.

## Tests

```bash
python -I -m unittest discover -s tools/pose_bridge -v
```
