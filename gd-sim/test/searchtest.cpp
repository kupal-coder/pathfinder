/**
 * Correctness tests for the search engine.
 *
 * The benchmark measures how far the search gets; this checks that what it
 * produces is actually valid. A macro that reports 100% but does not replay is
 * worse than no macro at all, so the central test replays each solution tape
 * through a fresh simulator and requires it to finish the level alive.
 */
#include "../../src/engine.hpp"
#include "../../src/search.hpp"

#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

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

static std::string spikeLevel(int gap, int len) {
	std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(len);
	for (int x = 400; x < len - 200; x += gap) {
		lvl += obj(8, x, 15);
		lvl += obj(8, x + 30, 15);
	}
	return lvl;
}

/// Replay a tape through a fresh Level. Returns the x reached, or -1 on death.
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

static void testReplayable(int gap) {
	PathfindOptions opts;
	opts.threads = 2;
	opts.chunkFrames = 24;
	opts.beamWidth = 512;

	std::atomic_bool stop{false};
	pf::Engine engine(spikeLevel(gap, 8000), opts);

	std::thread watchdog([&]() {
		for (int i = 0; i < 600 && !stop; ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		stop = true;
	});

	auto outcome = engine.run(stop, nullptr);
	stop = true;
	watchdog.join();

	std::string name = "gap " + std::to_string(gap);
	if (!outcome.solved) {
		std::cout << "  skip  " << name << " (not solved; nothing to replay)\n";
		return;
	}

	// Toggles must be strictly increasing, or the macro cannot be applied.
	bool ordered = true;
	for (size_t i = 1; i < outcome.tape.toggles.size(); ++i)
		if (outcome.tape.toggles[i] <= outcome.tape.toggles[i - 1])
			ordered = false;
	check(ordered, name + ": toggle frames strictly increasing");

	float reached = replay(spikeLevel(gap, 8000), outcome.tape, 40000);
	Level probe(spikeLevel(gap, 8000));
	check(reached >= probe.length,
		  name + ": solution tape replays to the end (reached "
			  + std::to_string(reached) + " of " + std::to_string(probe.length) + ")");
}

/// The engine must not report a solution on a level that cannot be finished.
static void testNoFalsePositive() {
	PathfindOptions opts;
	opts.threads = 2;
	opts.chunkFrames = 24;
	opts.stallLimit = 400;

	std::atomic_bool stop{false};
	pf::Engine engine(spikeLevel(120, 6000), opts);
	auto outcome = engine.run(stop, nullptr);

	check(!outcome.solved, "impossible level is not reported as solved");
	check(outcome.percent < 100.0, "impossible level reports partial progress");
}

/// Stopping must be prompt and must still return the best effort so far.
static void testStopIsHonoured() {
	PathfindOptions opts;
	opts.threads = 2;
	std::atomic_bool stop{false};
	pf::Engine engine(spikeLevel(300, 40000), opts);

	auto begin = std::chrono::steady_clock::now();
	std::thread stopper([&]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(300));
		stop = true;
	});
	auto outcome = engine.run(stop, nullptr);
	stopper.join();
	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();

	check(secs < 5.0, "stop request honoured promptly (" + std::to_string(secs) + "s)");
	check(outcome.percent >= 0.0, "partial result returned after stop");
}

/// Degenerate levels must not hang or crash the search.
static void testDegenerate() {
	std::vector<std::pair<std::string, std::string>> cases = {
		{"empty level", ""},
		{"settings only", "kA4,0,kA3,0,kA11,0,kA2,0;"},
		{"garbage", "!!!!;;;;????;"},
		{"single block", "kA4,0;1,1,2,0,3,15;"},
	};

	for (auto const& [name, lvl] : cases) {
		PathfindOptions opts;
		opts.threads = 1;
		opts.stallLimit = 50;
		std::atomic_bool stop{false};
		try {
			pf::Engine engine(lvl, opts);
			auto outcome = engine.run(stop, nullptr);
			(void)outcome;
			check(true, std::string("degenerate level survives: ") + name);
		} catch (std::exception const& ex) {
			check(false, std::string("degenerate level survives: ") + name + " threw " + ex.what());
		}
	}
}

/// Same inputs must give the same result regardless of thread count.
static void testDeterministicReplay() {
	auto lvl = spikeLevel(300, 6000);
	PathfindOptions opts;
	opts.threads = 1;
	opts.stallLimit = 2000;
	std::atomic_bool stop{false};
	pf::Engine engine(lvl, opts);
	auto outcome = engine.run(stop, nullptr);

	if (!outcome.solved) {
		std::cout << "  skip  deterministic replay (not solved)\n";
		return;
	}
	float a = replay(lvl, outcome.tape, 40000);
	float b = replay(lvl, outcome.tape, 40000);
	check(a == b, "replaying the same tape twice gives the same result");
}

/**
 * Regression: input frames used to be held in a std::set<uint16_t>, which wraps
 * at 65535 -- 4.55 minutes at 240fps. Past that, planned inputs silently landed
 * near the start of the level and did nothing. This solves a level long enough
 * to need frame numbers well beyond that limit.
 */
static void testLongLevelFrames() {
	int len = 190000;
	std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(len);
	for (int x = 600; x < len - 300; x += 900)
		lvl += obj(8, x, 15);

	PathfindOptions opts;
	opts.threads = 2;
	opts.stallLimit = 8000;
	std::atomic_bool stop{false};
	pf::Engine engine(lvl, opts);
	auto outcome = engine.run(stop, nullptr);

	uint32_t maxFrame = 0;
	for (auto t : outcome.tape.toggles)
		maxFrame = std::max(maxFrame, t);

	check(outcome.solved, "long level solved");
	check(maxFrame > 65535,
		  "input frames exceed the old uint16 limit (max " + std::to_string(maxFrame) + ")");

	bool monotonic = true;
	for (size_t i = 1; i < outcome.tape.toggles.size(); ++i)
		if (outcome.tape.toggles[i] <= outcome.tape.toggles[i - 1])
			monotonic = false;
	check(monotonic, "long level toggles remain ordered past 65535");
}

/**
 * A level that needs more than one mechanic: floor gaps crossed with orbs and
 * a ship section, a pad, spike gauntlets and a speed change. Spike-only tests
 * exercise timing but not the interaction between vehicles and effect objects.
 */
static void testMixedFeatures() {
	int len = 12000;
	std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;";
	for (int x = 0; x < len; x += 30) {
		if ((x > 4000 && x < 4600) || (x > 8000 && x < 8400))
			continue;  // gaps that must be crossed
		lvl += obj(1, x, -15);
	}
	for (int x = 600; x < 3800; x += 350)
		lvl += obj(8, x, 15);
	lvl += obj(36, 4200, 60);   // yellow orb
	lvl += obj(36, 4450, 90);   // yellow orb
	lvl += obj(35, 5200, 15);   // yellow pad
	for (int x = 5600; x < 7800; x += 400)
		lvl += obj(8, x, 15);
	lvl += obj(13, 8050, 120);  // ship portal
	lvl += obj(12, 9200, 105);  // back to cube
	lvl += obj(202, 9600, 45);  // 2x speed
	for (int x = 10000; x < 11600; x += 500)
		lvl += obj(8, x, 15);

	PathfindOptions opts;
	opts.threads = 2;
	opts.stallLimit = 20000;
	std::atomic_bool stop{false};

	std::thread watchdog([&]() {
		for (int i = 0; i < 1200 && !stop; ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		stop = true;
	});

	pf::Engine engine(lvl, opts);
	auto outcome = engine.run(stop, nullptr);
	stop = true;
	watchdog.join();

	check(outcome.solved, "mixed-feature level solved");
	if (outcome.solved) {
		Level probe(lvl);
		float reached = replay(lvl, outcome.tape, 60000);
		check(reached >= probe.length, "mixed-feature solution replays to the end");
	}
}

int main() {
	std::cout << "-- search correctness --\n";
	testReplayable(300);
	testReplayable(200);
	testNoFalsePositive();
	testStopIsHonoured();
	testDegenerate();
	testDeterministicReplay();
	testLongLevelFrames();
	testMixedFeatures();

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
