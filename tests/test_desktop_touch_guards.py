"""Static integration guards; no app launch, build, settings access, or input injection."""
from pathlib import Path
import json
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def source(name):
    return (ROOT / name).read_text(encoding="utf-8")


class DesktopTouchGuards(unittest.TestCase):
    def test_settings_default_off(self):
        prefs = source("app/settings/streamingpreferences.cpp")
        for name in ("desktoptouchtoolbar", "desktoptouchdrag"):
            self.assertIn(f'settings.value("{name}", false)', prefs)

    def test_windows_only_opt_in(self):
        inputs = source("app/streaming/input/input.cpp")
        self.assertIn("if (m_DesktopToolbarEnabled || m_DesktopTrackpadEnabled)", inputs)
        self.assertIn("if (m_DesktopTouch && m_DesktopTouch->finger(event)) return;", inputs)
        self.assertIn("if (m_AbsoluteTouchMode)", inputs)
        self.assertIn("#ifdef Q_OS_WIN32", source("app/streaming/input/desktoptouch.h"))

    def test_finger_mapping_and_pen_bypass(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        self.assertNotIn("LiSendTouchEvent", code)
        self.assertIn('SDL_strcmp(name, "pen") == 0', code)
        self.assertIn("return false; // Preserve the existing native pen path.", code)
        self.assertIn("StreamUtils::scaleSourceToDestinationSurface", code)
        self.assertIn("LiSendMousePositionEvent", code)
        self.assertIn("LiSendMouseMoveAsMousePositionEvent", code)

    def test_stationary_anchor_never_starts_direct_press_on_timer(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        self.assertIn("m_DirectPending && m_DirectMoved && event->type == SDL_FINGERMOTION", code)
        block = code.split("void DesktopTouchController::tick()", 1)[1].split("void DesktopTouchController::action", 1)[0]
        self.assertNotIn("m_DirectPending", block)
        self.assertIn("position(m_DownX, m_DownY);\n                sink(DesktopTouch::DragPolicy::Press)", code)

    def test_drag_ends_on_lift_without_lock_timeout(self):
        code = source("app/streaming/input/touchpolicy.h")
        self.assertNotIn("Locked", code)
        self.assertNotIn("Resume", code)
        self.assertNotIn("5000", code)
        self.assertIn("if (state == Dragging) { state = Idle; sink(Release); }", code)

    def test_direct_multifinger_drag_accumulates_absolute_position(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        block = code.split("void DesktopTouchController::multiAction", 1)[1].split("bool DesktopTouchController::finger", 1)[0]
        self.assertIn("m_MultiPosition = {output.x, output.y}", block)
        direct = block.split("if (m_AbsoluteTouch) {", 1)[1].split("else {", 1)[0]
        self.assertIn("m_MultiPosition.x + output.x", direct)
        self.assertIn("position(float(m_MultiPosition.x", direct)
        self.assertNotIn("move(output", direct)

    def test_toolbar_consumes_touch_and_does_not_activate(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        for guard in ("WS_EX_NOACTIVATE", "MA_NOACTIVATE", "CloseTouchInputHandle", "SWP_NOACTIVATE", "DesktopTouch::isPointerPromotedMouse"):
            self.assertIn(guard, code)
        self.assertNotIn("HTTRANSPARENT", code)
        self.assertNotIn("SetForegroundWindow", code)

    def test_input_diagnostics_precede_device_filter_and_are_bounded(self):
        code = source("app/streaming/input/input.cpp").split("void SdlInputHandler::handleTouchFingerEvent", 1)[1]
        self.assertLess(code.index("observeFinger"), code.index("!= SDL_TOUCH_DEVICE_DIRECT"))
        diagnostics = source("app/streaming/input/desktoptouch.cpp").split("void DesktopTouchController::logInputDiagnostics", 1)[1].split("bool DesktopTouchController::ready", 1)[0]
        self.assertIn("m_DiagnosticLines >= 64", diagnostics)
        self.assertIn("< 5000", diagnostics)
        self.assertIn("maxContacts=", diagnostics)
        self.assertIn("lastCancel=", diagnostics)
        for private in ("event->x", "event->y", "fingerId", "touchId"):
            self.assertNotIn(private, diagnostics)

    def test_actions_and_modifier_bounds(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        self.assertEqual(code.count("i >= 20 && i <= 22 && m_Modifiers[i - 20]"), 2)
        reset = code.split("void DesktopTouchController::reset()", 1)[1].split("void DesktopTouchController::position", 1)[0]
        self.assertNotIn("index ==", reset)
        for index in (23, 24):
            self.assertIn(f"if (index == {index})", code)
        self.assertIn("m_DesktopTouch->reset()", source("app/streaming/input/keyboard.cpp"))

    def test_local_keyboard_and_unicode_labels(self):
        code = source("app/streaming/input/desktoptouch.cpp")
        for guard in ("GetSystemDirectoryW", "ShellExecuteW", 'L"\\\\osk.exe"'):
            self.assertIn(guard, code)
        for forbidden in ("SendInput(", "keybd_event(", "RegSetValue", "runas", "system("):
            self.assertNotIn(forbidden, code)
        literals = re.findall(r'L("(?:[^"\\]|\\.)*")', code)
        labels = [json.loads(value) for value in literals]
        self.assertIn("키보드", labels)
        self.assertIn("전체 해제", labels)
        self.assertIn("\n한 번", labels)
        self.assertIn("\n고정", labels)
        self.assertFalse(any("??" in label for label in labels))
        qml = source("app/gui/SettingsView.qml")
        self.assertIn("Windows 터치 단축키 바", qml)
        self.assertNotIn('Windows ??', qml)

    def test_ci_gate_precedes_application_build_and_packaging(self):
        ci = source(".github/workflows/build-win-mac.yml")
        self.assertLess(ci.index("scripts\\test-desktop-touch.bat"), ci.index("Build x64 binaries"))
        self.assertNotIn("continue-on-error", ci)
        script = source("scripts/test-desktop-touch.bat")
        self.assertIn("/UNDEBUG", script)
        self.assertIn("if errorlevel 1 exit /b 1", script)
        self.assertIn("test_desktop_touch_guards.py", script)


if __name__ == "__main__":
    unittest.main(verbosity=2)
