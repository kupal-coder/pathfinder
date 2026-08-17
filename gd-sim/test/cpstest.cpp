/**
 * Click-rate tests.
 *
 * Two things are being checked:
 *
 *  1. The accumulator produces clicks at a fixed 1/70 s cadence regardless of
 *     how time is fed to it, so the macro does not depend on frame rate.
 *  2. No macro the search actually produces contains two toggles closer than
 *     the cap allows -- checked on the finished tape, not just within an
 *     expansion chunk, since a chunk boundary is where a naive limiter leaks.
 */
#include "../../src/cps.hpp"
#include "../../src/engine.hpp"
#include "../../src/search.hpp"

#include <atomic>
#include <cmath>
#include <iostream>
#include <string>
#include <thread>

static int g_failures = 0;

static void check(bool cond, std::string const& what) {
	std::cout << (cond ? "  ok    " : "  FAIL  ") << what << "\n";
	if (!cond)
		++g_failures;
}

static std::string obj(int id, double x, double y) {
	return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y) + ";";
}

static std::string ground(int len, double y = -15) {
	std::string s;
	for (int x = 0; x < len; x += 30)
		s += obj(1, x, y);
	return s;
}

int main() {
	std::cout << "-- click rate limits --\n";

	check(pf::kMaxCPS == 70.0, "cap is 70 CPS");
	check(std::abs(pf::kClickTimestep - 1.0 / 70.0) < 1e-12, "timestep is exactly 1/70 s");

	// At 240 Hz, 240/70 = 3.43 frames. Rounding up to 4 keeps the actual rate
	// at 60 CPS, under the cap; rounding down to 3 would be 80 CPS, over it.
	int gap = pf::minToggleInterval(240.0);
	check(gap == 4, "240 Hz gives a 4-frame minimum gap");
	check(240.0 / gap <= 70.0, "resulting rate stays at or under 70 CPS");

	// The interval is derived from time, so a different physics rate rescales.
	check(pf::minToggleInterval(120.0) == 2, "120 Hz gives a 2-frame gap");
	check(120.0 / pf::minToggleInterval(120.0) <= 70.0, "120 Hz stays under cap");
	check(pf::minToggleInterval(1000.0) == 15, "1000 Hz gives a 15-frame gap");
	check(1000.0 / pf::minToggleInterval(1000.0) <= 70.0, "1000 Hz stays under cap");

	// Spider, swing and UFO treat a click as a discrete action.
	check(pf::clickIsDiscreteAction(VehicleType::Spider), "spider clicks are actions");
	check(pf::clickIsDiscreteAction(VehicleType::Swing), "swing clicks are actions");
	check(pf::clickIsDiscreteAction(VehicleType::Ufo), "UFO clicks are actions");
	check(!pf::clickIsDiscreteAction(VehicleType::Cube), "cube clicks are a held state");
	check(!pf::clickIsDiscreteAction(VehicleType::Ball), "ball clicks are a held state");
	check(!pf::clickIsDiscreteAction(VehicleType::Robot), "robot clicks are a held state");

	std::cout << "\n-- fixed timestep accumulator --\n";

	// One second of time must yield exactly 70 slots, however it is sliced.
	{
		pf::ClickAccumulator acc;
		int slots = 0;
		for (int i = 0; i < 240; ++i)
			slots += acc.advance(1.0 / 240.0);
		check(slots == 70, "240 even steps of 1/240 s yield 70 clicks");
	}
	{
		pf::ClickAccumulator acc;
		int slots = 0;
		for (int i = 0; i < 60; ++i)
			slots += acc.advance(1.0 / 60.0);
		check(slots == 70, "60 even steps of 1/60 s also yield 70 clicks");
	}
	{
		// Uneven slices, as if the frame rate were stuttering. The total is
		// what matters: the cadence must not depend on frame pacing.
		pf::ClickAccumulator acc;
		int slots = 0;
		double fed = 0.0;
		double pattern[] = {1.0 / 240, 1.0 / 30, 1.0 / 500, 1.0 / 45};
		int i = 0;
		while (fed < 1.0) {
			double dt = pattern[i++ % 4];
			if (fed + dt > 1.0)
				dt = 1.0 - fed;
			fed += dt;
			slots += acc.advance(dt);
		}
		check(slots == 70, "irregular frame pacing still yields 70 clicks per second");
	}
	{
		pf::ClickAccumulator acc;
		acc.advance(1.0 / 140.0);  // half a slot
		check(acc.timeToNextSlot() > 0.0, "partial progress reports time remaining");
		check(acc.advance(1.0 / 140.0) == 1, "completing the slot emits one click");
	}

	std::cout << "\n-- produced macros respect the cap --\n";

	// A level dense enough to tempt the search into rapid clicking.
	auto tightLevel = [](int gapUnits, int len) {
		std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(len);
		for (int x = 400; x < len - 200; x += gapUnits) {
			lvl += obj(8, x, 15);
			lvl += obj(8, x + 30, 15);
		}
		return lvl;
	};

	int minAllowed = pf::minToggleInterval();

	for (int spacing : {300, 200, 150}) {
		auto lvl = tightLevel(spacing, 12000);

		PathfindOptions opts;
		opts.threads = 2;
		opts.stallLimit = 5000;
		std::atomic_bool stop{false};

		std::thread watchdog([&]() {
			for (int i = 0; i < 600 && !stop; ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			stop = true;
		});

		pf::Engine engine(lvl, opts);
		auto outcome = engine.run(stop, nullptr);
		stop = true;
		watchdog.join();

		uint32_t tightest = outcome.tape.tightestGap();
		std::string label = "gap " + std::to_string(spacing);

		if (outcome.tape.toggles.size() < 2) {
			std::cout << "  skip  " << label << " (too few toggles to measure)\n";
			continue;
		}

		check(tightest >= static_cast<uint32_t>(minAllowed),
			  label + ": tightest toggle gap " + std::to_string(tightest)
				  + " frames respects the " + std::to_string(minAllowed) + "-frame cap");

		double impliedCps = 240.0 / static_cast<double>(tightest);
		check(impliedCps <= 70.0,
			  label + ": implied rate " + std::to_string(static_cast<int>(impliedCps))
				  + " CPS is at or under 70");
	}

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
