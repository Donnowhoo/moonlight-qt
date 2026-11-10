#include "desktoptouch.h"

#ifdef Q_OS_WIN32
#include "input.h"
#include "streaming/streamutils.h"
#include <Limelight.h>
#include <SDL_syswm.h>
#include <SDL_system.h>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <shellapi.h>
#include <windowsx.h>

namespace {
const wchar_t* labels[] = {
    L"\uc774\ub3d9", L"\ub354\ubcf4\uae30", L"\ud0a4\ubcf4\ub4dc", L"\ubcf5\uc0ac", L"\ubd99\uc5ec\ub123\uae30", L"\uc2e4\ud589\ucde8\uc18c",
    L"\uc798\ub77c\ub0b4\uae30", L"\ub2e4\uc2dc\uc2e4\ud589", L"\uc800\uc7a5", L"\uc804\uccb4\uc120\ud0dd", L"Esc \ucde8\uc18c", L"Tab \uc774\ub3d9",
    L"Enter", L"\uc0ad\uc81c", L"F2 \uc774\ub984", L"\u2190 \uc67c\ucabd", L"\u2191 \uc704", L"\u2192 \uc624\ub978\ucabd",
    L"\u2193 \uc544\ub798", L"\uc6b0\ud074\ub9ad\n\ub2e4\uc74c \ud130\uce58", L"Ctrl", L"Shift", L"Alt", L"\ud130\uce58 \ubc29\uc2dd", L"\uc804\uccb4 \ud574\uc81c", L"\uc9c4\ub2e8 \ub85c\uadf8"
};
constexpr unsigned modifierKeys[] = {0xA2, 0xA0, 0xA4};
constexpr int buttonCount = sizeof(labels) / sizeof(labels[0]);
}

DesktopTouchController::DesktopTouchController(SdlInputHandler& input, SDL_Window* window,
        bool toolbar, bool trackpad, bool absoluteTouch, int width, int height)
    : m_Input(input), m_Window(window), m_Toolbar(toolbar), m_Trackpad(trackpad),
      m_AbsoluteTouch(absoluteTouch), m_StreamWidth(width), m_StreamHeight(height)
{
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info) || info.subsystem != SDL_SYSWM_WINDOWS) {
        m_Input.writeDesktopTouchDiagnostic("Desktop touch input: initialization failed at window-info");
        return;
    }
    m_Owner = info.info.win.window;
    QSettings settings;
    m_X = settings.value("desktoptouch/x", 0.02).toDouble();
    m_Y = settings.value("desktoptouch/y", 0.02).toDouble();
    if (!std::isfinite(m_X)) m_X = 0.02;
    if (!std::isfinite(m_Y)) m_Y = 0.02;
    m_X = std::clamp(m_X, 0.0, 1.0);
    m_Y = std::clamp(m_Y, 0.0, 1.0);
    m_Collapsed = settings.value("desktoptouch/collapsed", true).toBool();

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MoonlightDesktopTouchBar";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        m_Input.writeDesktopTouchDiagnostic(QString("Desktop touch input: window-class failure=%1").arg(GetLastError()));
        return;
    }
    m_Bar = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, wc.lpszClassName,
                           L"Moonlight touch shortcuts", WS_POPUP, 0, 0, 1, 1,
                           m_Owner, nullptr, wc.hInstance, this);
    if (!m_Bar) {
        m_Input.writeDesktopTouchDiagnostic(QString("Desktop touch input: toolbar-window failure=%1").arg(GetLastError()));
        return;
    }
    if (!RegisterTouchWindow(m_Bar, 0))
        m_Input.writeDesktopTouchDiagnostic(QString("Desktop touch input: toolbar-registration failure=%1").arg(GetLastError()));
    SDL_SetWindowsMessageHook(observeWindowsMessage, this);
    m_MessageObserverInstalled = true;
    layout();
    SDL_version version{};
    SDL_GetVersion(&version);
    m_Input.writeDesktopTouchDiagnostic(QString("Desktop touch input: SDL=%1.%2.%3 streamTouchRegistered=%4")
        .arg(version.major).arg(version.minor).arg(version.patch).arg(IsTouchWindow(m_Owner, nullptr) != FALSE));
}

DesktopTouchController::~DesktopTouchController()
{
    if (m_MessageObserverInstalled) SDL_SetWindowsMessageHook(nullptr, nullptr);
    logInputDiagnostics(true);
    reset();
    if (m_Bar) DestroyWindow(m_Bar);
}

void SDLCALL DesktopTouchController::observeWindowsMessage(void* userdata, void* window,
        unsigned int message, Uint64 wParam, Sint64 lParam)
{
    auto self = static_cast<DesktopTouchController*>(userdata);
    if (window != self->m_Owner) return; // Do not count contacts on the toolbar.
    ++self->m_NativeMessages;
    if (message == WM_KILLFOCUS) self->m_NativeContacts.clear();
    if (message == WM_POINTERDOWN || message == WM_POINTERUP || message == WM_POINTERUPDATE) {
        ++self->m_PointerMessages;
        self->m_DiagnosticDirty = true;
    }
    if (message != WM_TOUCH) return;
    unsigned count = LOWORD(wParam);
    std::vector<TOUCHINPUT> contacts(count);
    if (!GetTouchInputInfo(reinterpret_cast<HTOUCHINPUT>(static_cast<intptr_t>(lParam)), count,
                          contacts.data(), sizeof(TOUCHINPUT))) {
        ++self->m_NativeReadErrors;
    }
    else for (const auto& contact : contacts) {
        if (contact.dwFlags & TOUCHEVENTF_PEN) continue;
        auto id = std::make_pair(reinterpret_cast<uintptr_t>(contact.hSource), contact.dwID);
        if (contact.dwFlags & TOUCHEVENTF_DOWN) {
            ++self->m_NativeDown;
            self->m_NativeContacts.insert(id);
            self->m_NativeMax = std::max(self->m_NativeMax, static_cast<unsigned>(self->m_NativeContacts.size()));
        }
        if (contact.dwFlags & TOUCHEVENTF_MOVE) ++self->m_NativeMotion;
        if (contact.dwFlags & TOUCHEVENTF_UP) {
            ++self->m_NativeUp;
            self->m_NativeContacts.erase(id);
        }
    }
    // Observation only. SDL still owns dispatch and closes the touch handle.
    self->m_DiagnosticDirty = true;
}

void DesktopTouchController::observeFinger(SDL_TouchFingerEvent* event, bool direct)
{
    if (event->type == SDL_FINGERDOWN) ++m_ReceivedDown;
    else if (event->type == SDL_FINGERUP) ++m_ReceivedUp;
    else ++m_ReceivedMotion;
    if (!direct) ++m_RejectedDevice;
    m_DiagnosticDirty = true;
}

void DesktopTouchController::logInputDiagnostics(bool force)
{
    auto now = SDL_GetTicks();
    // At most 64 periodic summaries per session plus the final summary.
    if (!force && (!m_DiagnosticDirty || m_DiagnosticLines >= 64 || uint32_t(now - m_LastDiagnostic) < 5000)) return;
    char summary[1024]{};
    SDL_snprintf(summary, sizeof(summary),
        "Desktop touch input: down=%u motion=%u up=%u rejectedDevice=%u pen=%u disabled=%u maxContacts=%u "
        "wheel=%u drag=%u right=%u cancelled=%u lastCancel=%s ready=%d blocked=%d physical=%u toolbarContact=%d",
        m_ReceivedDown, m_ReceivedMotion, m_ReceivedUp, m_RejectedDevice, m_PenEvents, m_DisabledEvents,
        m_MaxContacts, m_WheelOutputs, m_DragOutputs, m_RightOutputs, m_Cancellations, m_LastCancelReason,
        int(ready()), int(m_Blocked), m_PhysicalButtons, int(m_PointerActive));
    m_Input.writeDesktopTouchDiagnostic(QString::fromUtf8(summary));
    m_Input.writeDesktopTouchDiagnostic(QString("Desktop touch native: down=%1 motion=%2 up=%3 maxContacts=%4 readErrors=%5 pointerMessages=%6 observedMessages=%7")
        .arg(m_NativeDown).arg(m_NativeMotion).arg(m_NativeUp).arg(m_NativeMax).arg(m_NativeReadErrors).arg(m_PointerMessages).arg(m_NativeMessages));
    ++m_DiagnosticLines;
    m_LastDiagnostic = now;
    m_DiagnosticDirty = false;
}

bool DesktopTouchController::ready() const
{
    return GetForegroundWindow() == m_Owner && !IsIconic(m_Owner) && m_Input.isCaptureActive();
}

char DesktopTouchController::modifiers() const
{
    char result = 0;
    if (m_Keys.held(0xA2) || m_Keys.held(0xA3)) result |= MODIFIER_CTRL;
    if (m_Keys.held(0xA0) || m_Keys.held(0xA1)) result |= MODIFIER_SHIFT;
    if (m_Keys.held(0xA4) || m_Keys.held(0xA5)) result |= MODIFIER_ALT;
    if (m_Keys.held(0x5B) || m_Keys.held(0x5C)) result |= MODIFIER_META;
    return result;
}

void DesktopTouchController::key(unsigned code, DesktopTouch::KeyOwners::Owner owner,
                                  bool down, char flags)
{
    int change = m_Keys.set(code, owner, down);
    if (down) m_KeyFlags[code] = flags;
    if (change) {
        char mods = modifiers();
        if (code == 0xA3 || code == 0xA5 || (code >= 0x21 && code <= 0x28) || code == 0x2E)
            mods |= MODIFIER_EXTENDED;
        LiSendKeyboardEvent2(static_cast<short>(0x8000 | code),
                             change > 0 ? KEY_ACTION_DOWN : KEY_ACTION_UP, mods, m_KeyFlags[code]);
    }
}

void DesktopTouchController::physicalKey(short code, char action, char mods, char flags)
{
    // Physical modifiers remain separate from toolbar modifiers. Never put toolbar
    // flags into SDL's shortcut detector (Ctrl+Alt+Shift+Q must remain physical).
    unsigned vk = static_cast<unsigned short>(code) & 0xFF;
    bool down = action == KEY_ACTION_DOWN;
    if (down && !ready()) return;
    int change = m_Keys.set(vk, DesktopTouch::KeyOwners::Physical, down);
    if (down) {
        m_KeyFlags[vk] = flags;
        m_KeyExtended[vk] = mods & MODIFIER_EXTENDED;
    }
    if (change) LiSendKeyboardEvent2(code, action, mods | modifiers(), flags);
    if (!down && !(vk >= 0xA0 && vk <= 0xA5) && !m_LeftDown) clearOneShot();
}

void DesktopTouchController::physicalMouse(int button, bool down)
{
    cancelGesture("physical-mouse");
    if (down) m_PhysicalButtons |= 1u << button;
    else m_PhysicalButtons &= ~(1u << button);
}

void DesktopTouchController::clearOneShot()
{
    for (unsigned i = 0; i < m_Modifiers.size(); i++) {
        if (m_Modifiers[i] == 1) {
            m_Modifiers[i] = 0;
            key(modifierKeys[i], DesktopTouch::KeyOwners::Toolbar, false);
        }
    }
    if (m_Bar) InvalidateRect(m_Bar, nullptr, FALSE);
}

void DesktopTouchController::gestureAction(DesktopTouch::DragPolicy::Action action)
{
    using D = DesktopTouch::DragPolicy;
    if (action == D::Click || action == D::DoubleClick) {
        int count = action == D::DoubleClick ? 2 : 1;
        int button = m_RightNext ? BUTTON_RIGHT : BUTTON_LEFT;
        m_RightNext = false;
        for (int i = 0; i < count; i++) {
            LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, button);
            LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, button);
        }
        clearOneShot();
    }
    else if (action == D::Press && !m_LeftDown) {
        m_LeftDown = true;
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, m_TouchButton);
    }
    else if (action == D::Release && m_LeftDown) {
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, m_TouchButton);
        m_LeftDown = false;
        if (!m_ChangingChord) clearOneShot();
    }
    if (m_Bar) InvalidateRect(m_Bar, nullptr, FALSE);
}

void DesktopTouchController::cancelGesture(const char* reason)
{
    if (!m_Fingers.empty() || m_LeftDown || m_MultiButton || m_DirectPending || m_Drag.holding()) {
        ++m_Cancellations;
        m_LastCancelReason = reason;
        m_DiagnosticDirty = true;
    }
    m_Gestures.cancel([this](DesktopTouch::TouchGestures::Output o) { multiAction(o); });
    m_DirectPending = false;
    m_WheelRemainder = 0;
    auto sink = [this](DesktopTouch::DragPolicy::Action a) { gestureAction(a); };
    m_Drag.cancel(sink);
    gestureAction(DesktopTouch::DragPolicy::Release);
    // Ignore the remainder of all contacts already on the glass.
    m_Blocked = !m_Fingers.empty();
    m_RemX = m_RemY = 0;
    m_RightNext = false;
}

void DesktopTouchController::reset()
{
    cancelGesture("reset-or-focus");
    m_Fingers.clear();
    m_ContactIds.clear();
    m_NextContact = 0;
    m_Gestures.reset([this](DesktopTouch::TouchGestures::Output o) { multiAction(o); });
    m_Blocked = false;
    m_PhysicalButtons = 0;
    for (unsigned code = 0; code < 256; code++) {
        if (m_Keys.held(code)) {
            LiSendKeyboardEvent2(static_cast<short>(0x8000 | code), KEY_ACTION_UP,
                                 m_KeyExtended[code], m_KeyFlags[code]);
        }
    }
    m_Keys.clear();
    m_Modifiers.fill(0);
    m_Actions.clear();
    m_Pressed = -1;
    m_PointerActive = false;
    m_TouchId = 0;
    if (m_Bar && GetCapture() == m_Bar) ReleaseCapture();
    if (m_Bar) InvalidateRect(m_Bar, nullptr, FALSE);
}

void DesktopTouchController::position(float x, float y)
{
    int w, h;
    SDL_GetWindowSize(m_Window, &w, &h);
    SDL_Rect src{0, 0, m_StreamWidth, m_StreamHeight}, dst{0, 0, w, h};
    StreamUtils::scaleSourceToDestinationSurface(&src, &dst);
    if (dst.w <= 0 || dst.h <= 0) return;
    // Send canvas-relative absolute mouse coordinates, not native touch. Sunshine
    // applies its per-monitor span map to this protocol path, including letterboxing.
    int px = std::clamp(int(x * w) - dst.x, 0, dst.w - 1);
    int py = std::clamp(int(y * h) - dst.y, 0, dst.h - 1);
    LiSendMousePositionEvent(short(px), short(py), short(dst.w), short(dst.h));
}

void DesktopTouchController::move(double dx, double dy)
{
    m_RemX += float(dx);
    m_RemY += float(dy);
    short mx = short(std::clamp(m_RemX, -32767.0f, 32767.0f));
    short my = short(std::clamp(m_RemY, -32767.0f, 32767.0f));
    m_RemX -= mx;
    m_RemY -= my;
    if (!mx && !my) return;
    if (m_Input.desktopAbsoluteMouseMode())
        LiSendMouseMoveAsMousePositionEvent(mx, my, short(m_StreamWidth), short(m_StreamHeight));
    else LiSendMouseMoveEvent(mx, my);
}

void DesktopTouchController::multiAction(DesktopTouch::TouchGestures::Output output)
{
    using G = DesktopTouch::TouchGestures;
    if (output.kind == G::Wheel) ++m_WheelOutputs;
    if (output.kind == G::LeftDown || output.kind == G::MiddleDown) ++m_DragOutputs;
    if (output.kind == G::RightClick) ++m_RightOutputs;
    m_DiagnosticDirty = true;
    if (m_AbsoluteTouch && (output.kind == G::LeftDown || output.kind == G::MiddleDown || output.kind == G::RightClick)) {
        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);
        double dipScale = m_Cell / 56.0;
        m_MultiPosition = {output.x, output.y};
        position(float(output.x * dipScale / std::max(w, 1)), float(output.y * dipScale / std::max(h, 1)));
    }
    if (output.kind == G::LeftDown || output.kind == G::MiddleDown) {
        m_MultiButton = output.kind == G::LeftDown ? BUTTON_LEFT : BUTTON_MIDDLE;
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, m_MultiButton);
    }
    else if (output.kind == G::LeftUp || output.kind == G::MiddleUp) {
        if (m_MultiButton) LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, m_MultiButton);
        m_MultiButton = 0;
        if (!m_ChangingChord) clearOneShot();
    }
    else if (output.kind == G::RightClick) {
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_RIGHT);
        clearOneShot();
    }
    else if (output.kind == G::Wheel) {
        // Natural vertical scrolling, 100 client DIPs = one 120-unit wheel notch.
        // Preserve fractions across events instead of quantizing to whole notches.
        m_WheelRemainder += output.y * 1.2;
        short amount = short(std::clamp(m_WheelRemainder, -32767.0, 32767.0));
        m_WheelRemainder -= amount;
        if (amount) LiSendHighResScrollEvent(amount);
    }
    else if (output.kind == G::Move) {
        int w, h;
        SDL_GetWindowSize(m_Window, &w, &h);
        double dipScale = m_Cell / 56.0;
        if (m_AbsoluteTouch) {
            // Accumulate in the same client coordinate space as the initial Down.
            // Absolute position packets bypass host pointer acceleration and use
            // exactly the same letterbox/span mapping as single-finger direct touch.
            m_MultiPosition.x = std::clamp(m_MultiPosition.x + output.x, 0.0, w / dipScale);
            m_MultiPosition.y = std::clamp(m_MultiPosition.y + output.y, 0.0, h / dipScale);
            position(float(m_MultiPosition.x * dipScale / std::max(w, 1)),
                     float(m_MultiPosition.y * dipScale / std::max(h, 1)));
        }
        else {
            move(output.x * dipScale * m_StreamWidth / std::max(w, 1),
                 output.y * dipScale * m_StreamHeight / std::max(h, 1));
        }
    }
    if (m_Bar) InvalidateRect(m_Bar, nullptr, FALSE);
}

bool DesktopTouchController::finger(SDL_TouchFingerEvent* event)
{
#if SDL_VERSION_ATLEAST(2, 0, 22)
    for (int i = 0; i < SDL_GetNumTouchDevices(); i++) {
        const char* name = SDL_GetTouchName(i);
        if (SDL_GetTouchDevice(i) == event->touchId && name && SDL_strcmp(name, "pen") == 0) {
            ++m_PenEvents;
            cancelGesture("pen");
            return false; // Preserve the existing native pen path.
        }
    }
#endif
    if (!(m_AbsoluteTouch ? m_Toolbar : m_Trackpad)) {
        ++m_DisabledEvents;
        return false;
    }
    auto id = std::make_pair(event->touchId, event->fingerId);
    auto sink = [this](DesktopTouch::DragPolicy::Action a) { gestureAction(a); };
    auto multiSink = [this](DesktopTouch::TouchGestures::Output o) { multiAction(o); };
    uint32_t now = SDL_GetTicks();
    int w, h;
    SDL_GetWindowSize(m_Window, &w, &h);
    double dipScale = m_Cell / 56.0;
    DesktopTouch::TouchGestures::Point point{event->x * w / dipScale, event->y * h / dipScale};
    if (event->type == SDL_FINGERDOWN) {
        if (m_Fingers.count(id)) return true;
        m_Fingers.insert(id);
        m_MaxContacts = std::max(m_MaxContacts, static_cast<unsigned>(m_Fingers.size()));
        m_ContactIds[id] = ++m_NextContact;
        m_ChangingChord = m_Fingers.size() > 1;
        m_Gestures.down(m_ContactIds[id], point, now, multiSink);
        m_ChangingChord = false;
        if (!ready() || m_PhysicalButtons || m_PointerActive) {
            const char* reason = !ready() ? "not-ready" : m_PhysicalButtons ? "physical-held" : "toolbar-contact";
            if (GetForegroundWindow() == m_Owner && !m_Input.isCaptureActive())
                m_Input.setCaptureActive(true); // First touch only restores capture, without a host click.
            cancelGesture(reason);
            return true;
        }
        if (m_Blocked) return true;
        if (m_Gestures.multi) {
            m_DirectPending = false;
            m_ChangingChord = true;
            m_Drag.cancel(sink);
            sink(DesktopTouch::DragPolicy::Release);
            m_ChangingChord = false;
            m_RemX = m_RemY = 0;
            return true;
        }
        m_Primary = id;
        m_DirectMoved = false;
        m_DownX = event->x;
        m_DownY = event->y;
        m_RemX = m_RemY = 0;
        if (m_AbsoluteTouch) {
            m_TouchButton = m_RightNext ? BUTTON_RIGHT : BUTTON_LEFT;
            m_RightNext = false;
            m_DirectPending = true; // Stationary contact never clicks before lift or meaningful motion.
        }
        else {
            m_TouchButton = BUTTON_LEFT;
            m_Drag.down(now, sink);
        }
    }
    else if (!m_Blocked && ready() && m_ContactIds.count(id) && m_Gestures.multi) {
        if (event->type == SDL_FINGERUP) m_Gestures.up(m_ContactIds[id], point, now, multiSink);
        else m_Gestures.motion(m_ContactIds[id], point, now, multiSink);
    }
    else if (!m_Blocked && ready() && id == m_Primary && m_Fingers.count(id)) {
        if (event->type == SDL_FINGERMOTION) m_Gestures.motion(m_ContactIds[id], point, now, multiSink);
        if (m_AbsoluteTouch) {
            m_DirectMoved |= std::hypot((event->x - m_DownX) * w / dipScale,
                                       (event->y - m_DownY) * h / dipScale) >= 8;
            if (m_DirectPending && m_DirectMoved && event->type == SDL_FINGERMOTION) {
                position(m_DownX, m_DownY);
                sink(DesktopTouch::DragPolicy::Press);
                m_DirectPending = false;
            }
            if (!m_DirectPending) position(event->x, event->y);
            if (event->type == SDL_FINGERUP) {
                if (m_DirectPending) {
                    position(event->x, event->y);
                    sink(DesktopTouch::DragPolicy::Press);
                    m_DirectPending = false;
                }
                sink(DesktopTouch::DragPolicy::Release);
            }
        }
        else {
            float dx = event->x - m_DownX, dy = event->y - m_DownY;
            m_Drag.motion(dx * dx + dy * dy > 0.0001f, sink);
            if (event->type == SDL_FINGERMOTION) {
                move(event->dx * m_StreamWidth, event->dy * m_StreamHeight);
            }
            if (event->type == SDL_FINGERUP) m_Drag.up(now, sink);
        }
    }
    if (event->type == SDL_FINGERUP) {
        if (m_ContactIds.count(id)) m_Gestures.up(m_ContactIds[id], point, now, multiSink);
        m_ContactIds.erase(id);
        m_Fingers.erase(id);
        if (m_Fingers.empty()) {
            m_Blocked = false;
            m_WheelRemainder = 0;
        }
    }
    return true;
}

void DesktopTouchController::tick()
{
    logInputDiagnostics();
    if (!ready()) {
        reset();
        layout();
        return;
    }
    auto now = SDL_GetTicks();
    m_Gestures.tick(now, [this](DesktopTouch::TouchGestures::Output o) { multiAction(o); });
    if (!m_Gestures.multi && !m_Blocked) {
        m_Drag.tick(now, [this](DesktopTouch::DragPolicy::Action a) { gestureAction(a); });
    }
    auto actions = std::move(m_Actions);
    m_Actions.clear();
    for (int index : actions) {
        if (ready()) action(index);
    }
    layout();
}

void DesktopTouchController::action(int index)
{
    if (index == 0) return;
    if (index == 25) {
        logInputDiagnostics(true);
        reset();
        const auto path = m_Input.desktopTouchDiagnosticPath().toStdWString();
        if (!path.empty()) ShellExecuteW(m_Owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (!m_Toolbar) { cancelGesture(); return; }
    if (index == 1) {
        m_Collapsed = !m_Collapsed;
        QSettings().setValue("desktoptouch/collapsed", m_Collapsed);
        layout();
        return;
    }
    if (index == 23) {
        reset();
        m_AbsoluteTouch = !m_AbsoluteTouch;
        m_Trackpad = true; // Explicit touch-only switch enables the enhanced path for this session.
        InvalidateRect(m_Bar, nullptr, FALSE);
        return;
    }
    if (index == 24) {
        reset();
        return;
    }
    cancelGesture();
    if (index == 2) {
        reset();
        // Explicit local-only OSK launch. No host hotkey, registry edit, shell
        // command, elevated verb, or automatic keyboard launch.
        wchar_t systemDirectory[MAX_PATH]{};
        UINT length = GetSystemDirectoryW(systemDirectory, MAX_PATH);
        if (length && length < MAX_PATH - 9) {
            wcscat_s(systemDirectory, L"\\osk.exe");
            auto result = ShellExecuteW(m_Owner, L"open", systemDirectory, nullptr, nullptr, SW_SHOWNORMAL);
            m_KeyboardFailed = reinterpret_cast<INT_PTR>(result) <= 32;
            if (reinterpret_cast<INT_PTR>(result) <= 32)
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Local Windows on-screen keyboard could not be opened");
        }
        return;
    }
    if (index >= 20 && index <= 22) {
        unsigned i = unsigned(index - 20);
        m_Modifiers[i] = (m_Modifiers[i] + 1) % 3;
        key(modifierKeys[i], DesktopTouch::KeyOwners::Toolbar, m_Modifiers[i] != 0);
    }
    else if (index == 19) m_RightNext = true;
    else {
        unsigned vk = 0;
        bool ctrl = index >= 3 && index <= 9;
        switch (index) {
        case 3: vk = 'C'; break; case 4: vk = 'V'; break; case 5: vk = 'Z'; break;
        case 6: vk = 'X'; break; case 7: vk = 'Y'; break; case 8: vk = 'S'; break;
        case 9: vk = 'A'; break; case 10: vk = 0x1B; break; case 11: vk = 0x09; break;
        case 12: vk = 0x0D; break; case 13: vk = 0x2E; break; case 14: vk = 0x71; break;
        case 15: vk = 0x25; break; case 16: vk = 0x26; break;
        case 17: vk = 0x27; break; case 18: vk = 0x28; break;
        default: return;
        }
        if (ctrl) key(0xA2, DesktopTouch::KeyOwners::Shortcut, true);
        key(vk, DesktopTouch::KeyOwners::Shortcut, true);
        key(vk, DesktopTouch::KeyOwners::Shortcut, false);
        if (ctrl) key(0xA2, DesktopTouch::KeyOwners::Shortcut, false);
        clearOneShot();
    }
    InvalidateRect(m_Bar, nullptr, FALSE);
}

void DesktopTouchController::layout()
{
    if (m_PointerActive) return;
    RECT owner{};
    GetClientRect(m_Owner, &owner);
    POINT origin{};
    ClientToScreen(m_Owner, &origin);
    // Stay on an actual monitor even when the stream spans a canvas with gaps.
    POINT anchor{origin.x + LONG(m_X * owner.right), origin.y + LONG(m_Y * owner.bottom)};
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromPoint(anchor, MONITOR_DEFAULTTONEAREST), &mi);
    RECT screen{origin.x, origin.y, origin.x + owner.right, origin.y + owner.bottom}, available{};
    if (!IntersectRect(&available, &screen, &mi.rcWork)) available = screen;
    HDC dc = GetDC(m_Bar);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(m_Bar, dc);
    // Runtime lookup keeps compatibility with older Windows SDK targets.
    using GetDpi = UINT(WINAPI*)(HWND);
    GetDpi getDpi = nullptr;
    auto dpiAddress = GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    static_assert(sizeof(getDpi) == sizeof(dpiAddress), "Windows function pointer size");
    std::memcpy(&getDpi, &dpiAddress, sizeof(getDpi));
    if (getDpi) dpi = int(getDpi(m_Bar));
    m_Cell = std::max(48, MulDiv(56, dpi, 96));
    int count = m_Toolbar ? (m_Collapsed ? 6 : buttonCount) : 2;
    m_Columns = std::max(1, std::min(6, int(available.right - available.left) / m_Cell));
    int width = std::min(count, m_Columns) * m_Cell;
    int height = ((count + m_Columns - 1) / m_Columns) * m_Cell;
    int x = std::clamp(int(anchor.x), int(available.left), std::max(int(available.left), int(available.right) - width));
    int y = std::clamp(int(anchor.y), int(available.top), std::max(int(available.top), int(available.bottom) - height));
    RECT current{};
    GetWindowRect(m_Bar, &current);
    if (current.left != x || current.top != y || current.right - current.left != width || current.bottom - current.top != height) {
        cancelGesture("toolbar-layout");
        SetWindowPos(m_Bar, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
        InvalidateRect(m_Bar, nullptr, FALSE);
    }
    ShowWindow(m_Bar, (m_Toolbar || m_Drag.holding() || m_MultiButton) && GetForegroundWindow() == m_Owner && !IsIconic(m_Owner)
               ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void DesktopTouchController::savePosition()
{
    RECT owner{}, bar{};
    POINT origin{};
    GetClientRect(m_Owner, &owner);
    ClientToScreen(m_Owner, &origin);
    GetWindowRect(m_Bar, &bar);
    m_X = std::clamp(double(bar.left - origin.x) / std::max(1L, owner.right), 0.0, 1.0);
    m_Y = std::clamp(double(bar.top - origin.y) / std::max(1L, owner.bottom), 0.0, 1.0);
    QSettings settings;
    settings.setValue("desktoptouch/x", m_X);
    settings.setValue("desktoptouch/y", m_Y);
}

int DesktopTouchController::hit(int x, int y) const
{
    RECT r{};
    GetClientRect(m_Bar, &r);
    if (x < 0 || y < 0 || x >= r.right || y >= r.bottom) return -1;
    int index = (y / m_Cell) * m_Columns + x / m_Cell;
    return index < (m_Toolbar ? (m_Collapsed ? 6 : buttonCount) : 2) ? index : -1;
}

void DesktopTouchController::pointer(int phase, int x, int y)
{
    if (phase == 0) {
        if (GetForegroundWindow() != m_Owner || m_PointerActive) return;
        if (!m_Input.isCaptureActive()) m_Input.setCaptureActive(true);
        m_PointerActive = true;
        m_PointerMoved = false;
        m_Pressed = hit(x, y);
        m_DragStart = {x, y};
        ClientToScreen(m_Bar, &m_DragStart);
        GetWindowRect(m_Bar, &m_StartRect);
        // Do not cancel pending clicks for move/collapse; all other toolbar actions
        // cancel the gesture before their remote input is dispatched on button-up.
        if (m_Pressed > 1) cancelGesture();
    }
    else if (m_PointerActive) {
        POINT point{x, y};
        ClientToScreen(m_Bar, &point);
        int dx = point.x - m_DragStart.x, dy = point.y - m_DragStart.y;
        m_PointerMoved |= dx * dx + dy * dy > 100;
        if (phase == 1 && m_Pressed == 0 && m_PointerMoved) {
            SetWindowPos(m_Bar, nullptr, m_StartRect.left + dx, m_StartRect.top + dy,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        if (phase == 2) {
            int pressed = m_Pressed;
            m_Pressed = -1;
            m_PointerActive = false;
            if (pressed == 0) {
                if (!m_PointerMoved && (m_Drag.holding() || m_MultiButton)) m_Actions.push_back(24);
                else savePosition();
            }
            else if (!m_PointerMoved && pressed >= 0 && pressed == hit(x, y)) m_Actions.push_back(pressed);
        }
    }
    InvalidateRect(m_Bar, nullptr, FALSE);
}

void DesktopTouchController::paint()
{
    PAINTSTRUCT ps{};
    HDC dc = BeginPaint(m_Bar, &ps);
    RECT all{};
    GetClientRect(m_Bar, &all);
    HBRUSH background = CreateSolidBrush(RGB(25, 30, 40));
    FillRect(dc, &all, background);
    DeleteObject(background);
    HFONT font = CreateFontW(-m_Cell / 5, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    auto oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    int count = m_Toolbar ? (m_Collapsed ? 6 : buttonCount) : 2;
    for (int i = 0; i < count; i++) {
        RECT r{(i % m_Columns) * m_Cell + 2, (i / m_Columns) * m_Cell + 2,
               (i % m_Columns + 1) * m_Cell - 2, (i / m_Columns + 1) * m_Cell - 2};
        bool active = i == m_Pressed || (i >= 20 && i <= 22 && m_Modifiers[i - 20]) || (i == 19 && m_RightNext);
        HBRUSH brush = CreateSolidBrush(active ? RGB(35, 100, 160) : RGB(48, 55, 68));
        FillRect(dc, &r, brush);
        DeleteObject(brush);
        SetTextColor(dc, RGB(245, 248, 255));
        std::wstring label = labels[i];
        if (i == 25 && m_Input.desktopTouchDiagnosticPath().isEmpty()) label = L"\ub85c\uadf8 \uc0dd\uc131\n\uc2e4\ud328";
        if (i == 0 && m_MultiButton) label = m_MultiButton == BUTTON_MIDDLE ? L"\uac00\uc6b4\ub370\n\ub04c\uae30" : L"\uc67c\ucabd\n\ub04c\uae30";
        if (i == 2 && m_KeyboardFailed) label = L"OSK\nError";
        if (i == 0 && m_Drag.holding()) label = L"\ub04c\uae30 \uc911\n\uc774\ub3d9";
        if (!m_Toolbar && i == 1) label = L"\uc804\uccb4 \ud574\uc81c";
        else if (i == 1 && !m_Collapsed) label = L"\uc811\uae30";
        if (i == 1 && (m_Modifiers[0] || m_Modifiers[1] || m_Modifiers[2])) label += L"\n\uc218\uc815\ud0a4 \ucf2c";
        if (i == 23) label = m_AbsoluteTouch ? L"\uc9c1\uc811 \ud130\uce58\n\u2192\ud2b8\ub799\ud328\ub4dc" : L"\ud2b8\ub799\ud328\ub4dc\n\u2192\uc9c1\uc811 \ud130\uce58";
        if (i >= 20 && i <= 22 && m_Modifiers[i - 20]) label += m_Modifiers[i - 20] == 1 ? L"\n\ud55c \ubc88" : L"\n\uace0\uc815";
        DrawTextW(dc, label.c_str(), -1, &r, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    }
    SelectObject(dc, oldFont);
    DeleteObject(font);
    EndPaint(m_Bar, &ps);
}

LRESULT CALLBACK DesktopTouchController::windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto self = reinterpret_cast<DesktopTouchController*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        self = static_cast<DesktopTouchController*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: self->paint(); return 0;
    case WM_TOUCH: {
        unsigned count = LOWORD(wp);
        std::vector<TOUCHINPUT> contacts(count);
        if (GetTouchInputInfo(reinterpret_cast<HTOUCHINPUT>(lp), count, contacts.data(), sizeof(TOUCHINPUT))) {
            for (const auto& contact : contacts) {
                POINT p{TOUCH_COORD_TO_PIXEL(contact.x), TOUCH_COORD_TO_PIXEL(contact.y)};
                ScreenToClient(hwnd, &p);
                if ((contact.dwFlags & TOUCHEVENTF_DOWN) && !self->m_TouchId && !self->m_PointerActive) {
                    self->m_TouchId = contact.dwID;
                    self->pointer(0, p.x, p.y);
                }
                else if (self->m_TouchId == contact.dwID) {
                    if (contact.dwFlags & TOUCHEVENTF_UP) {
                        self->pointer(2, p.x, p.y);
                        self->m_TouchId = 0;
                    }
                    else self->pointer(1, p.x, p.y);
                }
            }
        }
        CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(lp));
        return 0; // Consume the whole contact sequence; never forward to SDL/video.
    }
    case WM_LBUTTONDOWN:
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        // WM_TOUCH handles touch and pen. Their promoted mouse messages must
        // never take capture or start a second toolbar contact.
        if (DesktopTouch::isPointerPromotedMouse(static_cast<uintptr_t>(GetMessageExtraInfo()))) return 0;
        if (msg == WM_LBUTTONDOWN) {
            SetCapture(hwnd);
            self->pointer(0, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        }
        else if (msg == WM_LBUTTONUP) {
            self->pointer(2, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (GetCapture() == hwnd) ReleaseCapture();
        }
        else self->pointer(1, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
        self->m_PointerActive = false;
        self->m_Pressed = -1;
        self->m_TouchId = 0;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
#endif
