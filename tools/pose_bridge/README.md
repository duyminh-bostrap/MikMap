# pose_bridge

MediaPipe Pose sidecar for MikMap's **Camera skeleton** sensor: camera -> MediaPipe Pose
Landmarker -> one OSC bundle per frame over UDP. It runs as a separate process, so a crash
or a slow model cannot take the show down.

## Setup

```bash
python -m venv tools/pose_bridge/.venv
tools/pose_bridge/.venv/Scripts/activate        # Linux/macOS: source tools/pose_bridge/.venv/bin/activate
pip install -r tools/pose_bridge/requirements.txt
```

In MikMap: **Mapping › tree footer › Sensor › Skeleton OSC**. The popup picks a free OSC port
(the lowest port from 9000 up that no other sensor in the project uses, so often 9001+) and shows the exact command to run, with its
`--port`, for example:

```bash
python tools/pose_bridge/pose_bridge.py --port 9001   # first run downloads the .task model next to the script
```

Run that command from the repo root (venv active). The same command is shown later under the
camera's Properties › Connection.

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

`num-poses x joints` must be at most 64 (this bridge’s own limit; engine SensorFrame now supports 512), otherwise exit code 2.
Exit code 3 means the camera was lost (30 failed reads in a row). A line with fps and
latency is printed once per second.

## Tests

```bash
python -I -m unittest discover -s tools/pose_bridge -v
```

## Native alternative

MikMap also offers **Camera 2D → INPUT → Mode → Body**, using in-process RTMO
(s/m) with signed runtime/model packs; Off provides preview only. Windows capture
uses Media Foundation. Camera3D has no driver. See [pack templates](../packs/examples/README.md).
CPU fallback shows a yellow warning. An ONNX Runtime native crash can terminate
MikMap; keep this separate-process bridge for shows that must continue when the
inference process fails. Replay exercises recorded points, not inference.

The protocol above is the bridge’s incoming OSC protocol (z = MediaPipe landmark
z). It differs from MikMap’s **Send out** protocol: Camera2D sends optional
confidence z as `/mikmap/<device>/touch i:id f:x f:y f:z`; legacy xy remains `iff`.
First chunk has `/mikmap/<device>/frame i:count` (point count, not sequence or
people count). Large frames split into datagrams ≤1200 bytes; later chunks omit
that frame message. Text output uses `<device> frame <count>` and
`<device> <id> <x> <y> [<z>]`. UDP delivery/order is not guaranteed. The 1200-byte
chunk rule describes app Send out, not this bridge’s one-bundle-per-frame sender.
