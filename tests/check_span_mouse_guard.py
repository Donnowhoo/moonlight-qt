"""Source-level regression checks; does not build or launch Moonlight."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SpanMouseGuardTests(unittest.TestCase):
    def test_default_is_unlocked(self):
        header = (ROOT / "app/streaming/input/input.h").read_text()
        self.assertIn("bool m_AbsoluteMouseModeForced = false;", header)

    def test_force_locks_mode(self):
        source = (ROOT / "app/streaming/input/input.cpp").read_text()
        body = source.split("void SdlInputHandler::forceAbsoluteMouseMode()", 1)[1].split("}", 1)[0]
        self.assertIn("m_AbsoluteMouseModeForced = true;", body)
        self.assertIn("m_AbsoluteMouseMode = true;", body)

    def test_guard_precedes_capture_changes(self):
        source = (ROOT / "app/streaming/input/keyboard.cpp").read_text()
        body = source.split("case KeyComboToggleMouseMode:", 1)[1].split("case KeyComboToggleCursorHide:", 1)[0]
        guard, normal = body.split("// Uncapture input", 1)
        self.assertIn("if (m_AbsoluteMouseModeForced)", guard)
        self.assertIn("break;", guard)
        self.assertIn("m_AbsoluteMouseMode = !m_AbsoluteMouseMode;", normal)


if __name__ == "__main__":
    unittest.main()
