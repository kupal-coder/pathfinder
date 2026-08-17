#pragma once
#include <Level.hpp>
#include <Player.hpp>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

/**
 * Deterministic trace of a simulation run, used for golden-file regression.
 *
 * Two layers of protection, so the files stay small without losing coverage:
 *
 *   1. A rolling FNV-1a checksum folds in *every* field of *every* frame. Any
 *      drift anywhere -- a single ULP in slope ejection, a one-frame coyote
 *      change -- alters the final digest. Nothing is sampled away.
 *
 *   2. Human-readable lines are emitted for sampled frames and for every
 *      interesting transition (death, vehicle/gravity/size/speed change,
 *      grounded edges, slope enter/exit). This is what makes a failure
 *      diagnosable: the diff names a frame and shows the physics around it.
 *
 * Full per-frame text would be ~5 MB per run; this is ~10x smaller while the
 * checksum still covers all of it.
 */

namespace tracedetail {

struct Fnv {
    uint64_t h = 1469598103934665603ull;
    void bytes(void const* data, size_t n) {
        auto p = static_cast<unsigned char const*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    }
    /// Doubles are folded via their exact bit pattern -- no precision loss.
    void operator()(double v) { bytes(&v, sizeof(v)); }
    void operator()(float v) { double d = v; bytes(&d, sizeof(d)); }
    void operator()(int v) { bytes(&v, sizeof(v)); }
    void operator()(bool v) { unsigned char c = v ? 1 : 0; bytes(&c, 1); }
};

/// Fields that define an observable player state, in a fixed order.
inline void foldPlayer(Fnv& f, Player const& p) {
    f(p.pos.x); f(p.pos.y);
    f(p.size.x); f(p.size.y);
    f(p.velocity); f(p.acceleration); f(p.rotation);
    f(p.grounded); f(p.upsideDown); f(p.small); f(p.dead);
    f(p.speed); f(static_cast<int>(p.vehicle.type));
    f(p.slopeData.slope.has_value());
    f(p.button); f(p.input); f(p.buffer);
    f(static_cast<int>(p.coyoteFrames));
}

inline bool interesting(Player const& p, Player const& prev) {
    return p.dead
        || p.vehicle.type != prev.vehicle.type
        || p.upsideDown != prev.upsideDown
        || p.small != prev.small
        || p.speed != prev.speed
        || p.grounded != prev.grounded
        || p.slopeData.slope.has_value() != prev.slopeData.slope.has_value();
}

inline void writeLine(std::ostringstream& ss, int frame, Player const& p, char tag) {
    ss << tag << frame
       << " x " << p.pos.x
       << " y " << p.pos.y
       << " vel " << p.velocity
       << " acc " << p.acceleration
       << " rot " << p.rotation
       << " sx " << p.size.x
       << " sy " << p.size.y
       << " gnd " << (int)p.grounded
       << " ud " << (int)p.upsideDown
       << " sm " << (int)p.small
       << " sp " << p.speed
       << " veh " << (int)p.vehicle.type
       << " slope " << (int)p.slopeData.slope.has_value()
       << " dead " << (int)p.dead
       << "\n";
}

} // namespace tracedetail

/// Sample one readable line every N frames (transitions are always emitted).
inline constexpr int kTraceSampleRate = 16;

inline std::string traceRun(std::string const& levelString, std::string const& inputs) {
    using namespace tracedetail;

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(9);

    Level lvl(levelString);
    ss << "objects " << lvl.objectCount << " length " << lvl.length << "\n";

    Fnv digest;
    digest(static_cast<int>(lvl.objectCount));
    digest(lvl.length);

    Player prev = lvl.latestState();

    for (size_t i = 0; i < inputs.size(); ++i) {
        auto& p = lvl.runFrame(inputs[i] == '1');
        int frame = lvl.currentFrame();

        foldPlayer(digest, p);

        bool isEvent = interesting(p, prev);
        if (isEvent)
            writeLine(ss, frame, p, '!');
        else if (frame % kTraceSampleRate == 0)
            writeLine(ss, frame, p, 'f');

        prev = p;

        if (p.dead) {
            ss << "died at frame " << frame << "\n";
            break;
        }
    }

    ss << "end frame " << lvl.currentFrame() << "\n";
    ss << "digest " << std::hex << std::setw(16) << std::setfill('0') << digest.h << "\n";
    return ss.str();
}
