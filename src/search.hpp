#pragma once
#include <Level.hpp>
#include <Player.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

/**
 * Search primitives shared by the pathfinder and its tests.
 *
 * Kept free of Geode/GD headers so the whole search can be built and measured
 * as a plain command-line binary.
 */
namespace pf {

/// Compact record of the inputs that produced a state.
struct InputTape {
	/// Frames at which the button toggles. Strictly increasing.
	std::vector<uint32_t> toggles;

	void toggle(uint32_t frame) { toggles.push_back(frame); }

	/// Button state at `frame`, derived by counting toggles at or before it.
	bool pressedAt(uint32_t frame) const {
		size_t n = 0;
		for (auto t : toggles) {
			if (t > frame) break;
			++n;
		}
		return (n % 2) == 1;
	}

	size_t size() const { return toggles.size(); }
};

/**
 * Bucketed identity of a physics state.
 *
 * Two candidates that land in the same bucket are, for search purposes, the
 * same situation: continuing from either explores the same future. Collapsing
 * them is what stops the frontier filling with near-identical copies of one
 * trajectory -- the failure mode that made a naive beam collapse.
 */
struct StateKey {
	int32_t y;
	int32_t vel;
	uint32_t flags;

	bool operator==(StateKey const& o) const {
		return y == o.y && vel == o.vel && flags == o.flags;
	}
};

struct StateKeyHash {
	size_t operator()(StateKey const& k) const {
		uint64_t h = 1469598103934665603ull;
		auto mix = [&h](uint64_t v) {
			h ^= v;
			h *= 1099511628211ull;
		};
		mix(static_cast<uint64_t>(static_cast<uint32_t>(k.y)));
		mix(static_cast<uint64_t>(static_cast<uint32_t>(k.vel)));
		mix(k.flags);
		return static_cast<size_t>(h);
	}
};

/// Quantisation of the continuous state. Coarse enough to merge duplicates,
/// fine enough to keep genuinely different trajectories apart.
inline constexpr float kYBucket = 2.0f;
inline constexpr double kVelBucket = 4.0;

inline StateKey keyFor(Player const& p) {
	StateKey k{};
	k.y = static_cast<int32_t>(p.pos.y / kYBucket);
	k.vel = static_cast<int32_t>(p.velocity / kVelBucket);
	k.flags = static_cast<uint32_t>(p.vehicle.type)
			| (static_cast<uint32_t>(p.upsideDown) << 4)
			| (static_cast<uint32_t>(p.small) << 5)
			| (static_cast<uint32_t>(p.grounded) << 6)
			| (static_cast<uint32_t>(p.speed) << 7)
			| (static_cast<uint32_t>(p.slopeData.slope.has_value()) << 10);
	return k;
}

/**
 * A point the search can expand from.
 *
 * Holds a Level checkpoint rather than a full state history: with the bounded
 * ring buffer a checkpoint is a few kilobytes and copies in well under a
 * microsecond, which is what makes a wide frontier affordable.
 */
struct Candidate {
	Level::Checkpoint checkpoint;
	InputTape tape;
	bool pressed = false;
	float x = 0.0f;
	int frame = 0;
	/// Higher is better. See scoreState.
	double score = 0.0;
	/// Bucketed state identity, filled in on expansion.
	StateKey key {};
};

/**
 * Rank a state.
 *
 * Distance dominates, because progress is the goal. The correction terms matter
 * more than they look: ranking on distance alone treats a state that scraped
 * past a hazard with a pixel to spare as equal to a clean one, so the search
 * commits to trajectories it cannot continue from and then has to unwind them.
 *
 *  - airborne states are penalised slightly, since a grounded cube has more
 *    options available next frame;
 *  - extreme vertical speeds are penalised, as they tend to precede a ceiling
 *    or floor death that is not yet visible.
 */
inline double scoreState(Player const& p) {
	double score = p.pos.x;

	if (!p.grounded)
		score -= 1.5;

	double speedPenalty = std::abs(p.velocity) / 600.0;
	score -= speedPenalty * 2.0;

	return score;
}

/// A state the search should never expand.
inline bool isDoomed(Player const& p, float levelLength) {
	if (p.dead)
		return true;
	if (p.pos.y > 1300.0f || p.pos.y < -10.0f)
		return true;
	(void)levelLength;
	return false;
}

} // namespace pf
