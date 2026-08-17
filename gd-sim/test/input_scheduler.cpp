#include "input_scheduler.hpp"
#include <cassert>
#include <numeric>
#include <vector>

using namespace pathfinder;

int main() {
    // The count is exactly 70 regardless of render/physics cadence.
    for (double dt : {1. / 60., 1. / 120., 1. / 240., 1. / 360.}) {
        FixedTimestepAccumulator clock;
        unsigned ticks = 0;
        const int frames = static_cast<int>(1. / dt + .5);
        for (int i = 0; i < frames; ++i)
            ticks += clock.consume(dt);
        assert(ticks == 70);
    }

    // A hitch catches up rather than moving the clock's phase.
    FixedTimestepAccumulator variable;
    unsigned ticks = 0;
    for (double dt : std::vector<double>{.003, .007, .2, .19, .1, .5})
        ticks += variable.consume(dt);
    assert(ticks == 70);

    auto frames = fixedTickFrames(1, 240, 1. / 240.);
    assert(frames.size() == 70);
    for (size_t i = 1; i < frames.size(); ++i)
        assert(frames[i] > frames[i - 1]);

    assert(harmlessHoldSpam(ActionMode::Cube));
    assert(harmlessHoldSpam(ActionMode::Ball));
    assert(harmlessHoldSpam(ActionMode::Robot));
    assert(!harmlessHoldSpam(ActionMode::Spider));
    assert(!harmlessHoldSpam(ActionMode::Swing));

    ClickRateLimiter limiter;
    for (uint64_t frame = 1; frame <= 70; ++frame) {
        assert(limiter.accept(frame, false));
        assert(limiter.accept(frame, true)); // P2 has an independent cap.
    }
    assert(!limiter.accept(71, false));
    assert(!limiter.accept(71, true));

    // At exactly one 240 Hz window later the oldest press expires.
    assert(limiter.accept(241, false));
    assert(limiter.accept(241, true));

    ClickRateLimiter generatedLimiter;
    for (auto frame : fixedTickFrames(1, 2400, 1. / 240.))
        assert(generatedLimiter.accept(frame, false));
}
