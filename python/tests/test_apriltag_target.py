import unittest
import xml.etree.ElementTree as element_tree
from pathlib import Path


PROJECT_ROOT = Path(__file__).parents[2]
TARGET_PATH = (
    PROJECT_ROOT
    / "assets"
    / "apriltag"
    / "tagStandard41h12-id0-90mm-a4.svg"
)


class AprilTagTargetTests(unittest.TestCase):
    def test_printable_target_preserves_pose_span(self) -> None:
        root = element_tree.parse(TARGET_PATH).getroot()
        namespace = {"svg": "http://www.w3.org/2000/svg"}
        marker = root.find("svg:image", namespace)

        self.assertEqual(root.attrib["width"], "210mm")
        self.assertEqual(root.attrib["height"], "297mm")
        self.assertEqual(root.attrib["data-family"], "tagStandard41h12")
        self.assertEqual(root.attrib["data-id"], "0")
        self.assertEqual(root.attrib["data-total-cells"], "9")
        self.assertEqual(root.attrib["data-border-cells"], "5")
        self.assertEqual(root.attrib["data-tag-size-mm"], "90")
        self.assertIsNotNone(marker)

        rendered_width_mm = float(marker.attrib["width"])
        pose_span_mm = rendered_width_mm * 5.0 / 9.0
        self.assertAlmostEqual(pose_span_mm, 90.0)


if __name__ == "__main__":
    unittest.main()
