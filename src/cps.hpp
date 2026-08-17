#pragma once
#include <Player.hpp>
#include <Vehicle.hpp>
#include <algorithm>
#include <cstdint>

/**
 * Click-rate limiting.
 *
 * The physics runs at 240 Hz, so without a limit the search can emit a toggle
 * on every frame -- 240 clicks per second. That is not reproducible by a real
 * bot, and in the vehicles where a click is a discrete *action* it is actively
 * fatal.
 *
 * Two distinct rules, because the vehicles are not alike:
 *
 *  - Cube, ball, robot: holding is harmless. The button being down is a
 *    continuous state, so a fast repeat is at worst wasted input. 70 CPS is
 *    applied as a flat rate.
 *
 *  - Spider, swing (and UFO): every press is an event -- a teleport, a gravity
 *    flip, a flap. Spamming at a constant 70 CPS guarantees death, because the
 *    player flips continuously rather than when intended. Here 70 is a *cap*:
 *    the search may click far more slowly, and usually must.
 *
 * The interval is derived from a fixed timestep of 1/70 s rather than from the
 * frame rate, so the same macro is produced regardless of the physics dt.
 */
namespace pf {

/// Maximum clicks per second the search may emit.
inline constexpr double kMaxCPS = 70.0;

/// Fixed timestep the click generator advances on, independent of frame rate.
inline constexpr double kClickTimestep = 1.0 / kMaxCPS;

/**
 * Minimum frames between two toggles at a given physics rate.
 *
 * At the standard 240 Hz this is ceil(240/70) = 4 frames, i.e. 60 CPS actual --
 * the first achievable rate at or under the 70 cap. Rounding up rather than to
 * nearest matters: rounding to 3 frames would be 80 CPS and would breach it.
 */
inline int minToggleInterval(double physicsHz = 240.0) {
	double frames = physicsHz * kClickTimestep;
	int whole = static_cast<int>(frames);
	if (static_cast<double>(whole) < frames)
		++whole;
	return std::max(1, whole);
}

/// Vehicles where each click is a discrete action rather than a held state.
inline bool clickIsDiscreteAction(VehicleType type) {
	switch (type) {
		case VehicleType::Spider:
		case VehicleType::Swing:
		case VehicleType::Ufo:
			return true;
		default:
			return false;
	}
}

/**
 * Minimum spacing the search should respect for a given vehicle.
 *
 * For hold-style vehicles this is the flat rate limit. For action-style
 * vehicles it is still the same hard cap -- the search is free to go slower,
 * and the scoring is what discourages needless clicks -- but the cap is never
 * exceeded.
 */
inline int minToggleInterval(VehicleType type, double physicsHz = 240.0) {
	return minToggleInterval(physicsHz);
}

/**
 * Fixed-timestep click accumulator.
 *
 * Advanced by real elapsed time; reports how many click opportunities have
 * accrued. Decoupling this from the frame rate means a macro generated at one
 * physics rate is valid at another, and that a frame-rate hitch cannot silently
 * change the click pattern.
 */
class ClickAccumulator {
public:
	explicit ClickAccumulator(double cps = kMaxCPS)
		: m_step(1.0 / cps) {}

	/**
	 * Advance by `dt` seconds. Returns the number of whole click slots gained.
	 *
	 * The comparison carries a small tolerance because repeatedly summing a
	 * value like 1/240 does not land exactly on a second: 240 additions of
	 * 1/240 total 0.9999999999999977, which would leave the 70th slot of each
	 * second permanently just out of reach and quietly cost one click per
	 * second. The tolerance is a millionth of a step, far below any real timing
	 * difference but comfortably above the accumulated rounding error.
	 */
	int advance(double dt) {
		m_accumulated += dt;
		int slots = 0;
		double const epsilon = m_step * 1e-6;
		while (m_accumulated >= m_step - epsilon) {
			m_accumulated -= m_step;
			++slots;
		}
		if (m_accumulated < 0.0)
			m_accumulated = 0.0;
		return slots;
	}

	/// Seconds until the next click slot opens.
	double timeToNextSlot() const { return m_step - m_accumulated; }

	void reset() { m_accumulated = 0.0; }

	double step() const { return m_step; }

private:
	double m_step;
	double m_accumulated = 0.0;
};

} // namespace pf
