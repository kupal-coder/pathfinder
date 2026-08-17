/**
 * Per-feature tests.
 *
 * The acceptance criterion for this work is "the macro replays with zero
 * deaths". The only honest way to hold that line is to insist on two things:
 *
 *  1. Where the simulator claims a solve, the tape must replay to the end
 *     alive. That is checked here by replaying through a fresh simulator.
 *
 *  2. Where the simulator cannot model a feature, it must say so rather than
 *     produce a confident macro. A route computed against a world missing its
 *     geometry is worse than no route, because the user cannot tell it is
 *     wrong until it kills them.
 *
 * Levels marked `expectFullySupported = false` are therefore asserted to be
 * *detected* as unsupported, not asserted to be solved.
 */
#include "../../src/engine.hpp"
#include "../../src/search.hpp"
#include "levels.hpp"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>

static int g_failures = 0;

static void check(bool cond, std::string const& what) {
	std::cout << (cond ? "  ok    " : "  FAIL  ") << what << "\n";
	if (!cond)
		++g_failures;
}

/// Replay a tape through a fresh Level. Returns x reached, or -1 on death.
static float replay(std::string const& levelString, pf::InputTape const& tape, int maxFrames) {
	Level lvl(levelString);
	bool pressed = false;
	size_t next = 0;

	for (int f = 0; f < maxFrames; ++f) {
		uint32_t frame = static_cast<uint32_t>(lvl.currentFrame());
		while (next < tape.toggles.size() && tape.toggles[next] == frame) {
			pressed = !pressed;
			++next;
		}
		lvl.runFrame(pressed);
		if (lvl.latestState().dead)
			return -1.0f;
		if (lvl.latestState().pos.x >= lvl.length)
			return lvl.latestState().pos.x;
	}
	return lvl.latestState().pos.x;
}

int main() {
	std::cout << "-- feature detection --\n";

	for (auto const& tl : testlevels::all()) {
		Level probe(tl.data);
		bool supported = probe.fullySupported();

		if (tl.expectFullySupported) {
			std::string detail;
			if (!supported) {
				detail = " (" + probe.unknownObjects.summary();
				for (auto const& f : probe.unsupportedFeatures)
					detail += " " + f;
				detail += ")";
			}
			check(supported, tl.name + ": modelled exactly" + detail);
		} else {
			check(!supported, tl.name + ": correctly reported as not fully modelled");
		}
	}

	std::cout << "\n-- phasing regression --\n";
	{
		auto tl = testlevels::phasingWall();
		Level lvl(tl.data);

		// The wall must exist in the search world at all.
		size_t groundOnly = 3000 / 30;
		check(lvl.objectCount > groundOnly,
			  "unmodelled wall objects are present in the collision world ("
				  + std::to_string(lvl.objectCount) + " > " + std::to_string(groundOnly) + ")");

		// And it must stop the player rather than being walked through.
		bool died = false;
		for (int i = 0; i < 2000; ++i) {
			lvl.runFrame(false);
			if (lvl.latestState().dead) { died = true; break; }
		}
		check(died, "walking into the wall kills, rather than phasing through");
		check(lvl.latestState().pos.x < 1100.f,
			  "player stopped at the wall (x=" + std::to_string(lvl.latestState().pos.x) + ")");

		// And the level must be flagged, so no confident solve is claimed.
		check(!lvl.unknownObjects.empty(), "unmodelled ids are recorded, not silently dropped");
	}

	std::cout << "\n-- rotated object collision --\n";
	{
		// A spike rotated 45 degrees still occupies its centre, so a player
		// walking into it must die. Under the old axis-aligned approximation
		// the shape tested was wrong.
		auto tl = testlevels::rotatedObjects();
		Level lvl(tl.data);
		bool died = false;
		for (int i = 0; i < 3000; ++i) {
			lvl.runFrame(false);
			if (lvl.latestState().dead) { died = true; break; }
		}
		check(died, "walking into rotated spikes is fatal (they are not ignored)");
	}

	std::cout << "\n-- solving what is supported --\n";

	for (auto const& tl : testlevels::all()) {
		if (!tl.expectFullySupported)
			continue;

		PathfindOptions opts;
		opts.threads = 2;
		opts.stallLimit = 6000;
		std::atomic_bool stop{false};

		std::thread watchdog([&]() {
			for (int i = 0; i < 900 && !stop; ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			stop = true;
		});

		pf::Engine engine(tl.data, opts);
		auto outcome = engine.run(stop, nullptr);
		stop = true;
		watchdog.join();

		if (!outcome.solved) {
			std::cout << "  info  " << tl.name << ": not solved ("
					  << outcome.percent << "%)\n";
			continue;
		}

		// A claimed solve must replay to the end without dying. This is the
		// acceptance criterion, checked mechanically.
		Level probe(tl.data);
		float reached = replay(tl.data, outcome.tape, 80000);
		check(reached >= probe.length,
			  tl.name + ": claimed solve replays to the end with zero deaths");
	}

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
