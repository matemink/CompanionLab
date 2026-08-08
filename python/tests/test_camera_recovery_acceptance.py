import unittest

from camera_recovery_acceptance import (
    camera_is_reconnecting,
    camera_is_streaming,
)


class CameraRecoveryAcceptanceTests(unittest.TestCase):
    def test_streaming_requires_frames_and_restart_count(self) -> None:
        snapshot = {
            "camera": {
                "phase": "streaming",
                "received_frames": 21,
                "camera_restarts": 2,
                "error": "",
            }
        }
        self.assertTrue(camera_is_streaming(snapshot, 20, 2))
        self.assertFalse(camera_is_streaming(snapshot, 22, 2))
        self.assertFalse(camera_is_streaming(snapshot, 20, 3))

    def test_reconnecting_requires_a_visible_failure(self) -> None:
        snapshot = {
            "camera": {
                "phase": "reconnecting",
                "received_frames": 10,
                "camera_restarts": 1,
                "error": "GStreamer frame stalled for 2000 ms",
            }
        }
        self.assertTrue(camera_is_reconnecting(snapshot))
        snapshot["camera"]["error"] = ""
        self.assertFalse(camera_is_reconnecting(snapshot))


if __name__ == "__main__":
    unittest.main()
