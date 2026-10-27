"""Unit tests for the pure functions of pose_bridge (no mediapipe/cv2 needed)."""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pose_bridge as pb  # noqa: E402

# Same 40 bytes as the C++ test in tests/test_osc.cpp ("/mikmap/touch 16 0.5 0.25 0.0").
CPP_VECTOR = ("2f6d696b6d61702f746f756368000000" "2c69666666000000"
              "00000010" "3f000000" "3e800000" "00000000")


class PoseBridgeTest(unittest.TestCase):
    def test_vector_matches_cpp(self):
        self.assertEqual(pb.encode_msg("/mikmap/touch", [16, 0.5, 0.25, 0.0]).hex(), CPP_VECTOR)

    def test_bundle_header(self):
        m = pb.encode_msg("/mikmap/meta", [1, 2.0, 30.0])
        b = pb.encode_bundle([m])
        self.assertTrue(b.startswith(b"#bundle\0" + bytes(7) + b"\x01"))
        self.assertEqual(b[16:20], struct.pack(">i", len(m)))
        self.assertEqual(b[20:], m)

    def test_slots_stable_when_order_swaps(self):
        self.assertEqual(pb.assign_slots([(0.2, 0.5), (0.8, 0.5)], [(0.79, 0.5), (0.21, 0.5)]), [1, 0])

    def test_new_person_gets_lowest_free_slot(self):
        self.assertEqual(pb.assign_slots([None, (0.8, 0.5)], [(0.8, 0.5), (0.3, 0.5)]), [1, 0])

    def test_low_visibility_dropped_and_up_sent(self):
        person = [(0.0, 0.0, 0.0, 0.0)] * 33          # one (x, y, z, visibility) per landmark
        person[16] = (0.5, 0.5, 0.0, 0.9)
        person[15] = (0.4, 0.5, 0.0, 0.1)
        msgs, ids = pb.frame_messages({0: person}, [15, 16], 0.5, {15, 16, 116})
        self.assertEqual(ids, {16})
        ups = [m for m in msgs if m.startswith(b"/mikmap/touch/up\0")]
        self.assertEqual(sorted(struct.unpack(">i", m[-4:])[0] for m in ups), [15, 116])


if __name__ == "__main__":
    unittest.main()
