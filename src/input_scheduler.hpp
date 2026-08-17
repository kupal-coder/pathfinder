#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

namespace pathfinder {

inline constexpr double kMaxClicksPerSecond = 70.0;

// A framerate-independent fixed-step clock. consume() may return more than one
// tick after a long render frame; no elapsed time is discarded.
class FixedTimestepAccumulator {
    double m_step;
    double m_accumulator = 0.0;

public:
    explicit FixedTimestepAccumulator(double rate = kMaxClicksPerSecond)
      : m_step(1.0 / rate) {}

    void setPhase(double elapsed) {
        m_accumulator = std::fmod(elapsed, m_step);
        if (m_accumulator < 0)
            m_accumulator += m_step;
    }

    uint32_t consume(double dt) {
        m_accumulator += dt;
        uint32_t ticks = 0;
        // The epsilon only counters binary rounding at an exact boundary.
        while (m_accumulator + 1e-12 >= m_step) {
            m_accumulator -= m_step;
            ++ticks;
        }
        return ticks;
    }
};

enum class ActionMode {
    Cube,
    Ship,
    Ball,
    Ufo,
    Wave,
    Robot,
    Spider,
    Swing,
};

// Spider clicks teleport and swing clicks flip gravity. They may use all 70
// action slots when the search asks for them, but must never be auto-filled.
constexpr bool harmlessHoldSpam(ActionMode mode) {
    return mode == ActionMode::Cube || mode == ActionMode::Ball ||
           mode == ActionMode::Robot;
}

class ClickRateLimiter {
    std::array<std::deque<uint64_t>, 2> m_presses;
    uint64_t m_windowFrames;
    size_t m_limit;

public:
    explicit ClickRateLimiter(
        uint64_t physicsRate = 240,
        size_t limit = static_cast<size_t>(kMaxClicksPerSecond)
    ) : m_windowFrames(physicsRate), m_limit(limit) {}

    bool accept(uint64_t frame, bool player2) {
        auto& presses = m_presses[player2 ? 1 : 0];
        while (!presses.empty() && presses.front() + m_windowFrames <= frame)
            presses.pop_front();
        if (presses.size() >= m_limit)
            return false;
        presses.push_back(frame);
        return true;
    }
};

inline std::vector<uint32_t> fixedTickFrames(
    uint32_t firstFrame,
    uint32_t lastFrame,
    double physicsDt,
    double rate = kMaxClicksPerSecond
) {
    std::vector<uint32_t> frames;
    if (lastFrame < firstFrame)
        return frames;

    FixedTimestepAccumulator clock(rate);
    clock.setPhase(static_cast<double>(firstFrame - 1) * physicsDt);
    for (uint32_t frame = firstFrame; frame <= lastFrame; ++frame) {
        // A physics frame can cross several action boundaries if physicsDt is
        // unusually large. The frame is emitted once because a second toggle
        // on the same frame cancels itself.
        if (clock.consume(physicsDt) != 0)
            frames.push_back(frame);
    }
    return frames;
}

} // namespace pathfinder
