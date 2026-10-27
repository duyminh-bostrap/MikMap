"""MediaPipe Pose sidecar for MikMap's "Skeleton" sensor.

camera -> MediaPipe Pose Landmarker -> one OSC bundle per frame over UDP.
Wire protocol (read by engine/io OscSource):
  /mikmap/meta  i:frame f:latency_ms f:fps
  /mikmap/touch i:id f:x f:y f:z        id = slot * 100 + landmark
  /mikmap/touch/up i:id                 ids present last frame but not this one
Pure functions here need neither mediapipe nor cv2; main() imports them lazily.
"""
import argparse
import math
import os
import socket
import struct
import sys
import time
import urllib.request

KEY_JOINTS = [0, 11, 12, 15, 16, 23, 24, 27, 28]
MAX_TOUCHES = 64          # OscSource / SensorFrame capacity
MISSED_FRAMES_LIMIT = 30  # consecutive failed camera reads before giving up
MODEL_URL = ("https://storage.googleapis.com/mediapipe-models/pose_landmarker/"
             "pose_landmarker_{m}/float16/latest/pose_landmarker_{m}.task")


def _pad4(b: bytes) -> bytes:
    return b + bytes(-len(b) % 4)


def _osc_str(s: str) -> bytes:
    return _pad4(s.encode("ascii") + b"\0")


def encode_msg(address: str, args: list[int | float]) -> bytes:
    """One OSC message: int -> 'i', float -> 'f', big-endian, padded to 4."""
    tags = "".join("i" if isinstance(a, int) else "f" for a in args)
    body = b"".join(struct.pack(">i" if isinstance(a, int) else ">f", a) for a in args)
    return _osc_str(address) + _osc_str("," + tags) + body


def encode_bundle(msgs: list[bytes]) -> bytes:
    """OSC bundle with the 'immediately' timetag; each element is size-prefixed."""
    return b"#bundle\0" + bytes(7) + b"\x01" + b"".join(struct.pack(">i", len(m)) + m for m in msgs)


def assign_slots(prev: list[tuple[float, float] | None], centres: list[tuple[float, float]],
                 max_dist: float = 0.2) -> list[int]:
    """Keep each person's slot across frames: greedy nearest match into old slots
    within max_dist; anyone left over takes the lowest slot not yet used."""
    pairs = sorted((math.dist(c, p), ci, slot)
                   for ci, c in enumerate(centres)
                   for slot, p in enumerate(prev) if p is not None and math.dist(c, p) <= max_dist)
    out: list[int | None] = [None] * len(centres)
    used: set[int] = set()
    for _, ci, slot in pairs:
        if out[ci] is None and slot not in used:
            out[ci] = slot
            used.add(slot)
    for ci in range(len(centres)):
        if out[ci] is None:
            out[ci] = next(s for s in range(len(prev) + len(centres)) if s not in used)
            used.add(out[ci])
    return out  # type: ignore[return-value]


def frame_messages(people: dict[int, list[tuple[float, float, float, float]]], joints: list[int],
                   min_vis: float, prev_ids: set[int]) -> tuple[list[bytes], set[int]]:
    """Touch messages for visible joints, then touch/up for ids that vanished.
    people: slot -> landmarks as (x, y, z, visibility). Returns (msgs, ids now down)."""
    ids: dict[int, tuple[float, float, float]] = {}
    for slot, lms in people.items():
        for j in joints:
            x, y, z, vis = lms[j]
            if vis >= min_vis:
                ids[slot * 100 + j] = (x, y, z)
    msgs = [encode_msg("/mikmap/touch", [i, *map(float, p)]) for i, p in sorted(ids.items())]
    msgs += [encode_msg("/mikmap/touch/up", [i]) for i in sorted(prev_ids - ids.keys())]
    return msgs, set(ids)


def _ensure_model(model: str) -> str:
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"pose_landmarker_{model}.task")
    if os.path.exists(path):
        return path
    url = MODEL_URL.format(m=model)
    print(f"downloading {url}", file=sys.stderr)
    try:
        urllib.request.urlretrieve(url, path + ".part")
        os.replace(path + ".part", path)
    except OSError as e:  # URLError is an OSError
        print(f"model download failed: {e}\nsave the file from {url} next to this script", file=sys.stderr)
        sys.exit(1)
    return path


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--camera", type=int, default=0)
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--model", choices=["lite", "full", "heavy"], default="lite")
    ap.add_argument("--num-poses", type=int, default=1)
    ap.add_argument("--joints", choices=["all", "key"], default="all")
    ap.add_argument("--min-visibility", type=float, default=0.5)
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=9000)
    args = ap.parse_args()

    joints = list(range(33)) if args.joints == "all" else KEY_JOINTS
    if args.num_poses * len(joints) > MAX_TOUCHES:
        print(f"{args.num_poses} poses x {len(joints)} joints = {args.num_poses * len(joints)} touches, "
              f"over the limit of {MAX_TOUCHES}; lower --num-poses or use --joints key", file=sys.stderr)
        sys.exit(2)

    model_path = _ensure_model(args.model)
    import cv2
    import mediapipe as mp
    from mediapipe.tasks import python as mp_python
    from mediapipe.tasks.python import vision

    detector = vision.PoseLandmarker.create_from_options(vision.PoseLandmarkerOptions(
        base_options=mp_python.BaseOptions(model_asset_path=model_path),
        running_mode=vision.RunningMode.VIDEO, num_poses=args.num_poses))
    cap = cv2.VideoCapture(args.camera, cv2.CAP_DSHOW) if sys.platform == "win32" else cv2.VideoCapture(args.camera)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.host, args.port)
    prev_centres: list[tuple[float, float] | None] = []
    prev_ids: set[int] = set()
    frame = missed = fps_count = 0
    fps = 0.0
    t_fps = time.perf_counter()
    t0 = t_fps
    code = 0
    try:
        while True:
            ok, bgr = cap.read()
            if not ok:
                missed += 1
                if missed >= MISSED_FRAMES_LIMIT:
                    print(f"camera {args.camera} lost ({missed} failed reads in a row)", file=sys.stderr)
                    code = 3
                    break
                continue
            missed = 0
            t_cap = time.perf_counter()
            rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
            result = detector.detect_for_video(mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb),
                                               int((t_cap - t0) * 1000))
            poses = [[(l.x, l.y, l.z, l.visibility) for l in lms] for lms in result.pose_landmarks]
            centres = [((p[23][0] + p[24][0]) / 2, (p[23][1] + p[24][1]) / 2) for p in poses]
            slots = assign_slots(prev_centres, centres)
            prev_centres = [None] * (max(slots, default=-1) + 1)
            for s, c in zip(slots, centres):
                prev_centres[s] = c
            msgs, prev_ids = frame_messages(dict(zip(slots, poses)), joints, args.min_visibility, prev_ids)
            latency_ms = (time.perf_counter() - t_cap) * 1000
            meta = encode_msg("/mikmap/meta", [frame & 0x7FFFFFFF, latency_ms, fps])
            sock.sendto(encode_bundle([meta, *msgs]), dest)
            frame += 1
            fps_count += 1
            now = time.perf_counter()
            if now - t_fps >= 1.0:
                fps = fps_count / (now - t_fps)
                print(f"{fps:.1f} fps / {latency_ms:.1f} ms latency")
                fps_count, t_fps = 0, now
    except KeyboardInterrupt:
        pass
    finally:
        # Release whatever is still down so the app does not keep stale touches.
        if prev_ids:
            sock.sendto(encode_bundle([encode_msg("/mikmap/touch/up", [i]) for i in sorted(prev_ids)]), dest)
        cap.release()
    sys.exit(code)


if __name__ == "__main__":
    main()
