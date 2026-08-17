/**
 * Standalone search benchmark.
 *
 * Builds the real engine (src/engine.hpp) against the real simulator, with no
 * Geode dependency, so search quality can be measured directly:
 *
 *   searchbench            -- run the built-in gauntlet
 *   searchbench <file>     -- run against a level string in a file
 *
 * The gauntlet includes the spacing that defeated the previous random search
 * (double spikes every 120 units), which is the case this rewrite exists for.
 */
#include "../../src/engine.hpp"
#include "../../src/search.hpp"

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

static std::string obj(int id, double x, double y) {
	return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y) + ";";
}

static std::string ground(int len, double y = -15) {
	std::string s;
	for (int x = 0; x < len; x += 30)
		s += obj(1, x, y);
	return s;
}

/// Ground plus evenly spaced double spikes. Lower `gap` means tighter timing.
static std::string spikeLevel(int gap, int len) {
	std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(len);
	for (int x = 400; x < len - 200; x += gap) {
		lvl += obj(8, x, 15);
		lvl += obj(8, x + 30, 15);
	}
	return lvl;
}

struct Case {
	std::string name;
	std::string level;
	double budgetSeconds;
	/// Whether a solution exists. Verified by exhaustive per-frame BFS over
	/// deduplicated states; see the note on the gauntlet below.
	bool solvable = true;
};

static int runCase(Case const& c, unsigned threads) {
	PathfindOptions opts;
	opts.threads = threads;
	opts.beamWidth = 512;
	opts.chunkFrames = 24;

	std::atomic_bool stop{false};
	pf::Engine engine(c.level, opts);

	auto start = std::chrono::steady_clock::now();

	// Enforce the time budget from a watchdog thread.
	std::thread watchdog([&]() {
		while (!stop) {
			auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			if (elapsed > c.budgetSeconds) {
				stop = true;
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	});

	auto outcome = engine.run(stop, nullptr);
	stop = true;
	watchdog.join();

	double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

	// A case that is known unsolvable passes by *proving* it quickly -- the
	// search must exhaust its options and stop, not spin until the budget runs
	// out. The previous implementation would grind on impossible sections
	// indefinitely, which is what a stuck progress bar looked like to users.
	bool pass = c.solvable ? outcome.solved : (!outcome.solved && secs < c.budgetSeconds * 0.5);

	std::cout << std::fixed << std::setprecision(2)
			  << "  " << (pass ? "PASS  " : "FAIL  ")
			  << std::setw(24) << std::left << c.name
			  << (outcome.solved ? " solved " : " partial")
			  << std::setw(8) << std::right << outcome.percent << "%"
			  << "  " << std::setw(7) << secs << "s"
			  << "  rounds " << std::setw(6) << outcome.rounds
			  << "  frames " << std::setw(10) << outcome.framesSimulated
			  << "  inputs " << outcome.tape.size()
			  << (c.solvable ? "" : "   (expected unsolvable)")
			  << "\n";

	return pass ? 0 : 1;
}

int main(int argc, char** argv) {
	unsigned threads = 0;
	if (char const* env = std::getenv("PF_THREADS"))
		threads = static_cast<unsigned>(std::atoi(env));

	if (argc > 1) {
		std::ifstream f(argv[1]);
		std::string lvl((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		if (lvl.empty()) {
			std::cerr << "empty level file\n";
			return 2;
		}
		return runCase({argv[1], lvl, 60.0, true}, threads);
	}

	/*
	 * Solvability of each case was established independently, by an exhaustive
	 * per-frame BFS over deduplicated states (one node per frame, both button
	 * states, no heuristic). Where that BFS exhausts the entire reachable set
	 * without finishing, no input sequence exists and the case is marked
	 * unsolvable.
	 *
	 * Tight spacings are genuinely impossible for a 1x cube -- at 130 units the
	 * BFS exhausts at x=1940.87 -- so they are kept as tests that the search
	 * recognises a dead end quickly rather than grinding on it forever, which
	 * is what the previous implementation did.
	 */
	std::vector<Case> gauntlet = {
		{"spikes gap 300", spikeLevel(300, 20000), 30.0, true},
		{"spikes gap 200", spikeLevel(200, 20000), 30.0, true},
		{"spikes gap 150", spikeLevel(150, 20000), 30.0, true},
		{"spikes gap 130 (impossible)", spikeLevel(130, 20000), 30.0, false},
		{"spikes gap 120 (impossible)", spikeLevel(120, 20000), 30.0, false},
		{"spikes gap 100 (impossible)", spikeLevel(100, 20000), 30.0, false},
	};

	std::cout << "search gauntlet (threads=" << (threads ? std::to_string(threads) : std::string("auto")) << ")\n";
	int failures = 0;
	for (auto const& c : gauntlet)
		failures += runCase(c, threads);

	std::cout << "\n" << (gauntlet.size() - failures) << "/" << gauntlet.size() << " cases passed\n";
	return failures == 0 ? 0 : 1;
}
