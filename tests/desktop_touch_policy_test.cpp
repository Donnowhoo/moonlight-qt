#include "../app/streaming/input/touchpolicy.h"
#include "../app/streaming/input/touchgestures.h"
#include <cassert>
#include <iostream>
#include <vector>

using D = DesktopTouch::DragPolicy;
using G = DesktopTouch::TouchGestures;
using K = DesktopTouch::KeyOwners;

int main()
{
    int tests = 0;
    {
        D d; std::vector<D::Action> out;
        auto sink = [&](D::Action a) { out.push_back(a); };
        d.down(0, sink); d.up(50, sink); d.tick(229, sink);
        assert(out.empty()); d.tick(230, sink); d.tick(1000, sink);
        assert((out == std::vector<D::Action>{D::Click})); ++tests;
    }
    {
        D d; std::vector<D::Action> out;
        auto sink = [&](D::Action a) { out.push_back(a); };
        d.down(0, sink); d.up(40, sink); d.down(100, sink); d.tick(500, sink);
        assert(out.empty()); d.up(200, sink);
        assert((out == std::vector<D::Action>{D::DoubleClick})); ++tests;
    }
    {
        D d; std::vector<D::Action> out;
        auto sink = [&](D::Action a) { out.push_back(a); };
        d.down(0, sink); d.up(40, sink); d.down(100, sink); d.motion(true, sink);
        assert(d.state == D::Dragging); d.down(101, sink); // duplicate must retain ownership
        assert(d.state == D::Dragging); d.up(200, sink);
        assert(d.state == D::Idle);
        assert((out == std::vector<D::Action>{D::Press, D::Release}));
        d.down(300, sink); d.motion(true, sink); d.up(400, sink);
        assert(d.state == D::Idle); d.tick(5400, sink); d.cancel(sink);
        assert((out == std::vector<D::Action>{D::Press, D::Release})); ++tests;
    }
    {
        D d; std::vector<D::Action> out;
        auto sink = [&](D::Action a) { out.push_back(a); };
        d.down(0, sink); d.tick(649, sink); assert(out.empty()); d.tick(650, sink);
        d.up(700, sink); assert(d.state == D::Idle);
        d.tick(6000, sink);
        assert((out == std::vector<D::Action>{D::Press, D::Release})); ++tests;
    }
    for (int phase = 0; phase < 4; phase++) {
        D d; std::vector<D::Action> out;
        auto sink = [&](D::Action a) { out.push_back(a); };
        d.down(0, sink);
        if (phase == 1) d.up(10, sink);
        if (phase >= 2) d.tick(650, sink);
        if (phase == 3) d.up(700, sink);
        d.cancel(sink); d.cancel(sink); d.tick(10000, sink);
        assert(out.size() == (phase >= 2 ? 2u : 0u)); ++tests;
    }
    {
        D d; int clicks = 0;
        auto sink = [&](D::Action a) { if (a == D::Click) clicks++; };
        d.down(UINT32_MAX - 100, sink); d.up(UINT32_MAX - 50, sink); d.tick(129, sink);
        assert(clicks == 1); ++tests;
    }
    {
        D d; int actions = 0;
        auto sink = [&](D::Action) { actions++; };
        d.down(0, sink); d.motion(true, sink); d.tick(1000, sink); d.up(1100, sink);
        assert(actions == 0); ++tests;
    }
    // Both acquisition orders, each release order, all toolbar modifier VKs.
    for (unsigned vk : {0xA0, 0xA2, 0xA4}) {
        for (bool physicalFirst : {false, true}) {
            for (bool physicalReleaseFirst : {false, true}) {
                K keys;
                auto first = physicalFirst ? K::Physical : K::Toolbar;
                auto second = physicalFirst ? K::Toolbar : K::Physical;
                assert(keys.set(vk, first, true) == 1);
                assert(keys.set(vk, second, true) == 0);
                assert(keys.set(vk, K::Shortcut, true) == 0);
                assert(keys.set(vk, K::Shortcut, false) == 0);
                assert(keys.set(vk, physicalReleaseFirst ? K::Physical : K::Toolbar, false) == 0);
                assert(keys.set(vk, physicalReleaseFirst ? K::Toolbar : K::Physical, false) == -1);
                assert(keys.set(vk, first, false) == 0);
                assert(!keys.held(vk)); ++tests;
            }
        }
    }
    for (bool reverse : {false, true}) {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        int anchor = reverse ? 2 : 1, mover = reverse ? 1 : 2;
        g.down(anchor, {0, 0}, 0, sink); g.down(mover, {100, 0}, 10, sink);
        g.motion(mover, {120, 0}, 110, sink);
        assert(out.empty()); g.tick(150, sink);
        assert(g.state == G::AnchorDrag && out.size() == 1 && out[0].kind == G::LeftDown);
        assert(out[0].x == 100 && out[0].y == 0);
        g.motion(mover, {125, 2}, 160, sink);
        assert(out.back().kind == G::Move && out.back().x == 5 && out.back().y == 2);
        g.up(anchor, {0, 0}, 170, sink); assert(out.back().kind == G::LeftUp);
        auto size = out.size(); g.motion(mover, {140, 2}, 180, sink);
        g.up(mover, {140, 2}, 190, sink); g.cancel(sink);
        assert(out.size() == size); ++tests;
    }
    {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.down(2, {100, 0}, 10, sink);
        g.motion(1, {0, 10}, 50, sink); g.motion(2, {100, 10}, 60, sink);
        assert(out.size() == 1); g.motion(1, {0, 11}, 110, sink);
        assert(g.state == G::Scroll && out[0].kind == G::Wheel);
        g.motion(2, {100, 10.2}, 120, sink);
        assert(std::abs(out.back().y - 0.1) < 0.00001);
        g.up(1, {0, 11}, 150, sink); auto size = out.size();
        g.down(3, {100, 10}, 160, sink); g.up(3, {100, 10}, 170, sink);
        assert(out.size() == size); ++tests;
    }
    for (bool reverse : {false, true}) {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        int anchor = reverse ? 9 : 1, tap = reverse ? 1 : 9;
        g.down(anchor, {0, 0}, 0, sink); g.down(tap, {100, 0}, 20, sink);
        g.up(tap, {100, 0}, 80, sink); assert(g.state == G::WaitTap);
        g.down(tap + 10, {101, 1}, 150, sink); g.up(tap + 10, {101, 1}, 210, sink);
        assert(out.size() == 1 && out[0].kind == G::RightClick);
        g.motion(anchor, {0, 100}, 230, sink); g.up(anchor, {0, 100}, 250, sink);
        assert(out.size() == 1); ++tests;
    }
    for (int failure = 0; failure < 4; failure++) {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.down(2, {100, 0}, 10, sink);
        g.up(2, {100, 0}, 50, sink);
        if (failure == 0) g.motion(1, {9, 0}, 60, sink);
        g.down(3, {failure == 1 ? 125.0 : 100.0, 0}, failure == 2 ? 400 : 100, sink);
        if (failure == 3) g.motion(3, {109, 0}, 110, sink);
        g.up(3, {100, 0}, failure == 2 ? 420 : 150, sink);
        assert(out.empty()); ++tests;
    }
    for (bool fromDrag : {false, true}) {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.down(2, {100, 0}, 10, sink);
        if (fromDrag) { g.motion(2, {120, 0}, 110, sink); g.tick(150, sink); }
        g.down(3, {200, 0}, 160, sink);
        if (fromDrag) assert(out.back().kind == G::LeftUp);
        auto size = out.size(); g.up(3, {200, 0}, 200, sink);
        assert(out.size() == size); // Stationary triple is a no-op, never middle click or keyboard.
        ++tests;
    }
    for (bool cancel : {false, true}) {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.down(2, {100, 0}, 10, sink); g.down(3, {200, 0}, 20, sink);
        g.motion(1, {0, 40}, 40, sink);
        assert(out.size() == 1 && out.back().kind == G::MiddleDown);
        g.motion(2, {100, 3}, 50, sink);
        assert(out.back().kind == G::Move && out.back().y == 1);
        if (cancel) g.reset(sink); else g.up(2, {100, 3}, 60, sink);
        assert(out.back().kind == G::MiddleUp);
        auto size = out.size(); g.cancel(sink); assert(out.size() == size); ++tests;
    }
    {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.down(2, {100, 0}, 10, sink);
        g.motion(1, {0, 15}, 110, sink);
        assert(out.empty()); g.tick(130, sink); assert(out.empty());
        g.motion(2, {100, 15}, 135, sink); g.tick(200, sink);
        assert(g.state == G::Scroll && out.size() == 1 && out[0].kind == G::Wheel); ++tests;
    }
    {
        G g; std::vector<G::Output> out;
        auto sink = [&](G::Output o) { out.push_back(o); };
        g.down(1, {0, 0}, 0, sink); g.tick(3000, sink);
        assert(out.empty());
        g.down(2, {100, 0}, 3010, sink); g.up(2, {100, 0}, 3050, sink);
        g.down(3, {100, 0}, 3100, sink); g.up(3, {100, 0}, 3150, sink);
        assert(out.size() == 1 && out[0].kind == G::RightClick); ++tests;
    }
    std::cout << tests << " desktop touch policy scenarios passed\n";
}
