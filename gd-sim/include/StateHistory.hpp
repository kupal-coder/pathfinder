#pragma once
#include <cassert>
#include <cstddef>
#include <vector>

struct Player;
class Level;

/**
 * Bounded ring buffer of player states.
 *
 * The simulator used to keep *every* frame in a `std::vector<Player>` that grew
 * without limit. At 368 bytes per frame that is 110 MB on a long level, and
 * forking a search candidate meant copying all of it -- measured at 911 us per
 * fork versus 21 us to actually simulate the frames. Search was drowning in
 * memcpy.
 *
 * The physics only ever reads one frame backwards (`prevPlayer`) or one
 * forwards (`nextPlayer`). The one construct that can index arbitrarily far
 * back is `snapData.playerFrame` in Block::trySnap; instrumenting a long run
 * showed its real lookback is 1 frame. So a small window is sufficient, and
 * `kDefaultWindow` leaves a large safety margin over that.
 *
 * Unbounded mode is still available (capacity 0) for tools that genuinely want
 * the whole trace, such as the in-game debug overlay.
 */
class StateHistory {
public:
    /// Frames retained in bounded mode. Far above the 1-frame measured need.
    static constexpr size_t kDefaultWindow = 16;

    /// A restorable point in time. Small: window * sizeof(Player).
    struct Checkpoint {
        std::vector<Player> window;
        int count = 0;
        int oldest = 1;
    };

    StateHistory() = default;

    /// capacity 0 == unbounded. Must be set before any push.
    void setCapacity(size_t capacity) {
        m_cap = capacity;
        m_buf.clear();
        if (m_cap)
            m_buf.reserve(m_cap);
        m_count = 0;
    }

    size_t capacity() const { return m_cap; }
    bool bounded() const { return m_cap != 0; }

    void push(Player const& p) {
        if (!bounded()) {
            m_buf.push_back(p);
            m_count = static_cast<int>(m_buf.size());
            return;
        }
        if (m_buf.size() < m_cap)
            m_buf.push_back(p);
        else
            m_buf[static_cast<size_t>(m_count) % m_cap] = p;
        ++m_count;
    }

    /// Total frames ever pushed. Frame numbers are 1-based, so this is also
    /// the number of the most recent frame.
    int count() const { return m_count; }

    /// Oldest frame number still resident in the window.
    int oldestFrame() const {
        if (!bounded())
            return 1;
        int o = m_count - static_cast<int>(m_cap) + 1;
        return o < 1 ? 1 : o;
    }

    bool empty() const { return m_buf.empty(); }

    Player& back() { return slot(m_count); }
    Player const& back() const { return slot(m_count); }

    /**
     * Fetch a frame. Mirrors the original clamping behaviour: frame 0 yields
     * the oldest state, and a frame past the end yields the newest.
     *
     * Reading before the window is a programming error -- it means some physics
     * path reaches further back than the window allows. Rather than silently
     * returning wrong data, this trips an assert in debug builds and clamps in
     * release so a mistake degrades instead of corrupting.
     */
    Player const& at(int frame) const {
        if (frame >= m_count)
            return slot(m_count);
        int oldest = oldestFrame();
        // Frame 0 is a documented alias for "oldest available state".
        if (frame <= 0)
            return slot(oldest);
        if (frame < oldest) {
            assert(frame >= oldest && "StateHistory: read outside retained window");
            return slot(oldest);
        }
        return slot(frame);
    }

    /// Discard frames after `frame`. Only valid inside the retained window.
    void truncate(int frame) {
        if (frame < 1)
            frame = 1;
        if (frame > m_count)
            return;
        if (!bounded()) {
            m_buf.resize(static_cast<size_t>(frame));
            m_count = frame;
            return;
        }
        assert(frame >= oldestFrame() && "StateHistory: rollback outside retained window");
        m_count = frame;
    }

    /**
     * Repoint every retained state at `owner`.
     *
     * Player holds a back-pointer to its Level for prevPlayer()/getState().
     * When a checkpoint is restored into a *different* Level instance -- which
     * is exactly what parallel search does, one Level per worker -- those
     * pointers would otherwise still reference the Level the states came from
     * and read the wrong history. Called by Level::restore.
     */
    void rebind(Level* owner);

    void reset(Player const& initial) {
        m_buf.clear();
        m_count = 0;
        push(initial);
    }

    Checkpoint checkpoint() const {
        Checkpoint cp;
        cp.count = m_count;
        cp.oldest = oldestFrame();
        int first = cp.oldest;
        cp.window.reserve(static_cast<size_t>(m_count - first + 1));
        for (int f = first; f <= m_count; ++f)
            cp.window.push_back(slot(f));
        return cp;
    }

    void restore(Checkpoint const& cp) {
        m_buf.clear();
        m_count = 0;
        if (!bounded()) {
            // Unbounded restore can only rebuild what the checkpoint carried.
            for (auto const& p : cp.window)
                m_buf.push_back(p);
            m_count = static_cast<int>(m_buf.size());
            return;
        }
        m_buf.resize(m_cap);
        m_count = cp.count - static_cast<int>(cp.window.size());
        for (auto const& p : cp.window) {
            m_buf[static_cast<size_t>(m_count) % m_cap] = p;
            ++m_count;
        }
    }

private:
    Player& slot(int frame) {
        return bounded() ? m_buf[static_cast<size_t>(frame - 1) % m_cap]
                         : m_buf[static_cast<size_t>(frame - 1)];
    }
    Player const& slot(int frame) const {
        return bounded() ? m_buf[static_cast<size_t>(frame - 1) % m_cap]
                         : m_buf[static_cast<size_t>(frame - 1)];
    }

    std::vector<Player> m_buf;
    size_t m_cap = kDefaultWindow;
    int m_count = 0;
};
