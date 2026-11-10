#pragma once

#include <QtGlobal>
#ifdef Q_OS_WIN32
#include "SDL_compat.h"
#include "touchpolicy.h"
#include "touchgestures.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <set>
#include <utility>
#include <vector>

class SdlInputHandler;

// Created and destroyed on the SDL main thread. The popup never activates Qt.
class DesktopTouchController {
public:
    DesktopTouchController(SdlInputHandler& input, SDL_Window* window, bool toolbar,
                           bool trackpad, bool absoluteTouch, int width, int height);
    ~DesktopTouchController();
    bool valid() const { return m_Bar != nullptr; }
    bool finger(SDL_TouchFingerEvent* event);
    void observeFinger(SDL_TouchFingerEvent* event, bool direct);
    void tick();
    void reset();
    void cancelGesture(const char* reason = "action");
    void physicalKey(short key, char action, char modifiers, char flags);
    void physicalMouse(int button, bool down);
    void finishClick() { if (!m_LeftDown) clearOneShot(); }
private:
    static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
    void pointer(int phase, int x, int y);
    void paint();
    void layout();
    void savePosition();
    void action(int index);
    void key(unsigned key, DesktopTouch::KeyOwners::Owner owner, bool down, char flags = 0);
    char modifiers() const;
    void clearOneShot();
    void gestureAction(DesktopTouch::DragPolicy::Action action);
    void position(float x, float y);
    void move(double dx, double dy);
    void multiAction(DesktopTouch::TouchGestures::Output output);
    bool ready() const;
    int hit(int x, int y) const;
    void logInputDiagnostics(bool force = false);

    SdlInputHandler& m_Input;
    SDL_Window* m_Window;
    HWND m_Owner = nullptr;
    HWND m_Bar = nullptr;
    bool m_Toolbar, m_Trackpad, m_AbsoluteTouch;
    int m_StreamWidth, m_StreamHeight;
    DesktopTouch::KeyOwners m_Keys;
    std::array<char, 256> m_KeyFlags{};
    std::array<char, 256> m_KeyExtended{};
    std::array<int, 3> m_Modifiers{};
    DesktopTouch::DragPolicy m_Drag;
    DesktopTouch::TouchGestures m_Gestures;
    std::map<std::pair<SDL_TouchID, SDL_FingerID>, int> m_ContactIds;
    int m_NextContact = 0, m_MultiButton = 0;
    bool m_DirectPending = false;
    bool m_DirectMoved = false;
    double m_WheelRemainder = 0;
    DesktopTouch::TouchGestures::Point m_MultiPosition{};
    std::set<std::pair<SDL_TouchID, SDL_FingerID>> m_Fingers;
    std::pair<SDL_TouchID, SDL_FingerID> m_Primary{};
    bool m_Blocked = false, m_LeftDown = false, m_RightNext = false;
    unsigned m_PhysicalButtons = 0;
    bool m_KeyboardFailed = false;
    bool m_ChangingChord = false;
    int m_TouchButton = 1;
    float m_DownX = 0, m_DownY = 0;
    float m_RemX = 0, m_RemY = 0;
    bool m_Collapsed = true;
    double m_X = 0.02, m_Y = 0.02;
    int m_Cell = 56, m_Columns = 6, m_Pressed = -1;
    POINT m_DragStart{};
    RECT m_StartRect{};
    bool m_PointerActive = false, m_PointerMoved = false;
    DWORD m_TouchId = 0;
    std::vector<int> m_Actions;
    // Bounded counters only: never record coordinates, typed text or device IDs.
    unsigned m_ReceivedDown = 0, m_ReceivedMotion = 0, m_ReceivedUp = 0;
    unsigned m_RejectedDevice = 0, m_PenEvents = 0, m_DisabledEvents = 0;
    unsigned m_MaxContacts = 0, m_Cancellations = 0;
    unsigned m_WheelOutputs = 0, m_DragOutputs = 0, m_RightOutputs = 0;
    unsigned m_DiagnosticLines = 0;
    uint32_t m_LastDiagnostic = 0;
    bool m_DiagnosticDirty = false;
    const char* m_LastCancelReason = "none";
};
#endif
