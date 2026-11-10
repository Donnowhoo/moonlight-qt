#pragma once

#include <array>
#include <cstdint>

// No SDL, Qt, clock, or network dependencies: callers supply time and an output sink.
namespace DesktopTouch {
class KeyOwners {
public:
    enum Owner { Physical = 1, Toolbar = 2, Shortcut = 4 };
    // -1 = release, 0 = no wire transition, 1 = press.
    int set(unsigned key, Owner owner, bool down) {
        auto& value = keys.at(key);
        bool before = value != 0;
        if (down) value = static_cast<unsigned char>(value | owner);
        else value = static_cast<unsigned char>(value & ~owner);
        return before == (value != 0) ? 0 : (value ? 1 : -1);
    }
    bool held(unsigned key) const { return keys.at(key) != 0; }
    void clear() { keys.fill(0); }
private:
    std::array<unsigned char, 256> keys{};
};

class DragPolicy {
public:
    enum State { Idle, Contact, Pending, Second, Dragging };
    enum Action { Press, Release, Click, DoubleClick };
    State state = Idle;
    uint32_t since = 0;
    bool moved = false;

    bool holding() const { return state == Dragging; }
    template<class Sink> void tick(uint32_t now, Sink sink) {
        if (state == Pending && uint32_t(now - since) >= 180) {
            state = Idle;
            sink(Click);
        }
        else if ((state == Contact || state == Second) && !moved && uint32_t(now - since) >= 650) {
            state = Dragging;
            sink(Press);
        }
    }
    template<class Sink> void down(uint32_t now, Sink sink) {
        tick(now, sink);
        if (state != Idle && state != Pending) return;
        state = state == Pending ? Second : Contact;
        since = now;
        moved = false;
    }
    template<class Sink> void motion(bool outsideDeadzone, Sink sink) {
        moved |= outsideDeadzone;
        if (state == Second && moved) {
            state = Dragging;
            sink(Press);
        }
    }
    template<class Sink> void up(uint32_t now, Sink sink) {
        if (state == Dragging) { state = Idle; sink(Release); }
        else if (state == Second) {
            state = Idle;
            if (!moved && uint32_t(now - since) < 250) sink(DoubleClick);
        }
        else if (state == Contact && !moved && uint32_t(now - since) < 250) state = Pending;
        else state = Idle;
        since = now;
    }
    template<class Sink> void cancel(Sink sink) {
        bool release = holding();
        state = Idle;
        moved = false;
        if (release) sink(Release);
    }
};
}
