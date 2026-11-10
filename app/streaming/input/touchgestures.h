#pragma once

#include <cmath>
#include <cstdint>
#include <iterator>
#include <initializer_list>
#include <map>

namespace DesktopTouch {
// Coordinates are client DIPs. Output contains only intent, never OS/network calls.
class TouchGestures {
public:
    enum State { Single, Pair, Scroll, AnchorDrag, WaitTap, SecondTap, Triple, MiddleDrag, Consumed };
    enum Kind { LeftDown, LeftUp, MiddleDown, MiddleUp, Move, Wheel, RightClick };
    struct Output { Kind kind; double x = 0, y = 0; };
    struct Point { double x = 0, y = 0; };
    struct Contact { Point start, point; uint32_t down; };
    State state = Single;
    std::map<int, Contact> contacts;
    uint32_t since = 0;
    int anchor = -1, moving = -1;
    Point tapPoint{}, anchorPoint{};
    bool multi = false;
    int candidateAnchor = -1;
    uint32_t candidateSince = 0;

    static double distance(Point a, Point b) {
        return std::hypot(a.x - b.x, a.y - b.y);
    }
    template<class Sink> void release(Sink sink) {
        if (state == AnchorDrag) sink(Output{LeftUp});
        if (state == MiddleDrag) sink(Output{MiddleUp});
    }
    template<class Sink> void cancel(Sink sink) {
        release(sink);
        state = Consumed;
    }
    template<class Sink> void reset(Sink sink) {
        cancel(sink);
        contacts.clear();
        state = Single;
        multi = false;
    }
    template<class Sink> void tick(uint32_t now, Sink sink) {
        if ((state == WaitTap || state == SecondTap) && uint32_t(now - since) > 300)
            state = Consumed;
        if (state == Pair && candidateAnchor >= 0 && uint32_t(now - candidateSince) >= 40) {
            auto stationary = contacts.find(candidateAnchor);
            if (stationary != contacts.end() && distance(stationary->second.point, stationary->second.start) <= 8) {
                anchor = candidateAnchor;
                for (const auto& entry : contacts) if (entry.first != anchor) moving = entry.first;
                state = AnchorDrag;
                const auto& target = contacts.at(moving).start;
                sink(Output{LeftDown, target.x, target.y});
            }
            candidateAnchor = -1;
        }
    }
    template<class Sink> void down(int id, Point p, uint32_t now, Sink sink) {
        if (state == WaitTap || state == SecondTap) tick(now, sink);
        if (contacts.count(id)) return;
        if (contacts.empty()) { state = Single; multi = false; }
        contacts.emplace(id, Contact{p, p, now});
        if (state == Consumed) return; // A completed/cancelled chord stays consumed until all contacts lift.
        if (contacts.size() == 2) {
            multi = true;
            if (state == WaitTap) {
                if (distance(p, tapPoint) <= 24 && contacts.count(anchor) &&
                        distance(contacts.at(anchor).point, anchorPoint) <= 8) {
                    moving = id;
                    state = SecondTap;
                }
                else state = Consumed;
            }
            else if (state == Single) {
                state = Pair;
                candidateAnchor = -1;
                since = now;
                for (auto& entry : contacts) entry.second.start = entry.second.point;
            }
        }
        else if (contacts.size() == 3) {
            release(sink); // A third contact always ends ownership of a two-finger drag.
            state = Triple;
            candidateAnchor = -1;
            since = now;
            for (auto& entry : contacts) entry.second.start = entry.second.point;
        }
        else if (contacts.size() > 3) cancel(sink);
    }
    template<class Sink> void motion(int id, Point p, uint32_t now, Sink sink) {
        auto found = contacts.find(id);
        if (found == contacts.end()) return;
        Point previous = found->second.point;
        found->second.point = p;
        if ((state == WaitTap || state == SecondTap) &&
                (id == anchor ? distance(p, anchorPoint) > 8 : distance(p, found->second.start) > 8)) {
            state = Consumed;
        }
        else if (state == Pair && contacts.size() == 2) {
            auto a = contacts.begin(), b = std::next(a);
            double ay = a->second.point.y - a->second.start.y;
            double by = b->second.point.y - b->second.start.y;
            if (std::abs(ay) >= 8 && std::abs(by) >= 8 && ay * by > 0) {
                state = Scroll;
                sink(Output{Wheel, 0, (ay + by) / 2});
            }
            else {
                for (auto stationary : {a, b}) {
                    auto mover = stationary == a ? b : a;
                    if (distance(stationary->second.point, stationary->second.start) <= 8 &&
                            distance(mover->second.point, mover->second.start) >= 12) {
                        if (candidateAnchor != stationary->first) {
                            candidateAnchor = stationary->first;
                            candidateSince = now;
                        }
                        break;
                    }
                }
            }
        }
        else if (state == Scroll) sink(Output{Wheel, 0, (p.y - previous.y) / 2});
        else if (state == AnchorDrag && id == moving) sink(Output{Move, p.x - previous.x, p.y - previous.y});
        else if (state == Triple && contacts.size() == 3) {
            Point delta{};
            for (const auto& entry : contacts) {
                delta.x += (entry.second.point.x - entry.second.start.x) / 3;
                delta.y += (entry.second.point.y - entry.second.start.y) / 3;
            }
            if (std::hypot(delta.x, delta.y) >= 12) {
                state = MiddleDrag;
                Point center{};
                for (const auto& entry : contacts) {
                    center.x += entry.second.point.x / 3;
                    center.y += entry.second.point.y / 3;
                }
                sink(Output{MiddleDown, center.x, center.y}); // Rebase before sending motion.
            }
        }
        else if (state == MiddleDrag) sink(Output{Move, (p.x - previous.x) / 3, (p.y - previous.y) / 3});
        tick(now, sink); // Evaluate only after updating the latest contact (SDL reports them separately).
    }
    template<class Sink> void up(int id, Point p, uint32_t now, Sink sink) {
        auto found = contacts.find(id);
        if (found == contacts.end()) return;
        // Apply final position for tap validation, but do not select a new drag on lift.
        found->second.point = p;
        if (state == Pair && contacts.size() == 2) {
            auto other = contacts.begin();
            if (other->first == id) ++other;
            if (uint32_t(now - found->second.down) <= 220 &&
                    distance(p, found->second.start) <= 8 &&
                    distance(other->second.point, other->second.start) <= 8) {
                anchor = other->first;
                anchorPoint = other->second.point;
                tapPoint = p;
                since = now;
                state = WaitTap;
            }
            else state = Consumed;
        }
        else if (state == SecondTap && id == moving && contacts.count(anchor) &&
                 uint32_t(now - since) <= 300 && uint32_t(now - found->second.down) <= 220 &&
                 distance(p, tapPoint) <= 24 && distance(p, found->second.start) <= 8 &&
                 distance(contacts.at(anchor).point, anchorPoint) <= 8) {
            sink(Output{RightClick, p.x, p.y});
            state = Consumed;
        }
        else if (state != Single) { release(sink); state = Consumed; }
        contacts.erase(id);
        if (contacts.empty()) { state = Single; multi = false; }
    }
};
}
