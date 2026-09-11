#include <set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <Level.hpp>
#include <Physics.hpp>
#include <random>
#include <gdr/gdr.hpp>
#include "pathfinder.hpp"

class Replay2 : public gdr::Replay<Replay2, gdr::Input<"">> {
 public:
	Replay2() : Replay("Pathfinding Pro", 1) {
		this->author = "percjue";
		this->description = "Auto-generated macro by Pathfinding Pro";
		this->framerate = 240.0;
	}
};

struct Level2 : public Level {
	bool press = false;
	float highestY = 0;
	std::vector<int> interestingFrames; // pre-computed frames near objects

	using Level::Level;

	Level2(std::string const& lvlString) : Level(lvlString) {
		// Find highest y
		for (auto& i : sections) {
			for (auto& j : i) {
				highestY = std::max(highestY, j->pos.y);
			}
		}
		// Pre-compute interesting frames near objects
		buildInterestingFrames();
	}

	void buildInterestingFrames() {
		constexpr float windowRadius = 30.0f; // ±30 frames around each object
		constexpr float fps = 240.0f;

		std::set<int> frameSet; // avoid duplicates during building

		for (auto& section : sections) {
			for (auto& obj : section) {
				// Speed portals can change the encounter frame substantially. Add
				// candidates for every speed tier so the search does not depend on
				// the level's initial speed remaining active.
				for (float speed : PHYS_SPEEDS) {
					float encounterFrame = (obj->pos.x / speed) * fps;
					int startFrame = static_cast<int>(encounterFrame - windowRadius);
					int endFrame = static_cast<int>(encounterFrame + windowRadius);

					if (startFrame < 1) startFrame = 1;
					for (int f = startFrame; f <= endFrame; ++f) {
						frameSet.insert(f);
					}
				}
			}
		}

		// Convert to sorted vector for fast random access
		interestingFrames.assign(frameSet.begin(), frameSet.end());
	}
};

bool isLevelEnd(Level2& lvl) {
	return lvl.latestState().pos.x >= lvl.length;
}

	int tryInputs(Level2& lvl, std::vector<uint8_t> const& inputs, int currentBestFrame) {
	auto frame = lvl.currentFrame();
	auto press_before = lvl.press;

	// Save minimal state for fast restore instead of full rollback
	int savedStateCount = static_cast<int>(lvl.gameStates.size());
	auto savedPlayer = lvl.gameStates.back();

	constexpr int maxSimFrames = 1500;
	constexpr int stuckThreshold = 60;     // frames without forward progress = stuck
		int f = frame;
	int endFrame = frame + maxSimFrames;

	float lastProgressX = lvl.latestState().pos.x;
	int framesSinceProgress = 0;

	while (!lvl.gameStates.back().dead && f < endFrame) {
			if (f >= frame && f - frame < static_cast<int>(inputs.size()) && inputs[f - frame]) {
			lvl.press = !lvl.press;
		}
		lvl.runFrame(lvl.press);
		++f;

		// --- Early termination checks ---

		// 1. Stuck detection: no forward progress for too long
		float currentX = lvl.latestState().pos.x;
		if (currentX > lastProgressX + 0.5f) {
			lastProgressX = currentX;
			framesSinceProgress = 0;
		} else {
			++framesSinceProgress;
			if (framesSinceProgress >= stuckThreshold) {
				break; // stuck in place, abandon this path
			}
		}
	}

	int finalFrame = static_cast<int>(lvl.gameStates.size());
	float lastY = lvl.latestState().pos.y;

	// Fast restore: truncate vector and copy back one state
	lvl.gameStates.resize(savedStateCount);
	lvl.gameStates.back() = savedPlayer;
	lvl.press = press_before;

	if (lastY > 1300 || lastY < 0)
		return 0;
	return finalFrame;
}

namespace {

// A single catalogued "this was alive here" snapshot.
struct CheckpointNode {
	int frame;
	float x;
	Player state;
	int failCount = 0; // retries from this exact node without escalating further back
};

// All the state for one independent line of search. Several of these run
// concurrently on their own threads; periodically the ones that have fallen
// behind get pulled back up to whichever branch is currently in the lead.
struct SearchBranch {
	Level2 lvl;
	Level2 lvlBest;                       // this branch's own best-ever-reached state
	std::vector<CheckpointNode> checkpoints;
	std::mt19937 rng;
	int iterations = 100;
	int stuckStreak = 0;                  // consecutive failed passes, used to widen the backtrack gap
	float lastCatalogedPct = 0.0f;

	SearchBranch(Level2 const& seed, uint32_t seedVal)
		: lvl(seed), lvlBest(seed), rng(seedVal) {
		checkpoints.push_back({lvl.currentFrame(), lvl.latestState().pos.x, lvl.latestState()});
		lastCatalogedPct = pctOf(lvl.latestState().pos.x);
	}

	float pctOf(float x) const {
		return lvl.length > 0.0f ? (x / lvl.length) * 100.0f : 0.0f;
	}

	// Every time the sim survives to a new % of the level, its full state
	// gets catalogued. Anything that's fallen well behind current progress
	// gets dropped — e.g. once we're at 17%, the 1-9% checkpoints no longer
	// help us backtrack usefully. Index 0 always stays as a last-resort
	// anchor back to the very start of the level.
	void catalogAndPrune() {
		constexpr float catalogStepPct   = 1.0f;
		constexpr float catalogWindowPct = 10.0f;

		float pct = pctOf(lvl.latestState().pos.x);
		if (pct - lastCatalogedPct < catalogStepPct) return;

		checkpoints.push_back({lvl.currentFrame(), lvl.latestState().pos.x, lvl.latestState()});
		lastCatalogedPct = pct;

		float cutoffPct = pct - catalogWindowPct;
		if (checkpoints.size() > 1) {
			checkpoints.erase(
				std::remove_if(checkpoints.begin() + 1, checkpoints.end() - 1,
					[&](CheckpointNode const& c) { return pctOf(c.x) < cutoffPct; }),
				checkpoints.end() - 1);
		}

		// Failures heal while things are going well — a node that failed a
		// few times shouldn't be permanently written off.
		for (auto& cp : checkpoints) {
			if (cp.failCount > 0) cp.failCount--;
		}
	}

	// Replace this branch's active state wholesale — used both to catch a
	// straggler branch up to the current global leader, and to fall back to
	// this branch's own best-ever point when its local catalog runs dry.
	void resetTo(Level2 const& source) {
		lvl = source;
		checkpoints.clear();
		checkpoints.push_back({lvl.currentFrame(), lvl.latestState().pos.x, lvl.latestState()});
		lastCatalogedPct = pctOf(lvl.latestState().pos.x);
		stuckStreak = 0;
		iterations = 100;
	}

	// Called when a pass makes no forward progress at all. Rewinds to the
	// nearest checkpoint that has enough clearance before the death point to
	// give a different click room to matter, instead of just dying the same
	// way again. That clearance grows the longer we've been stuck here, and
	// a checkpoint gets skipped in favor of an earlier one once it's proven
	// to be a dead end too many times.
	void handleStuck() {
		constexpr int minBacktrackGap   = 40;  // frames of clearance on the very first retry
		constexpr int gapGrowthPerFail  = 15;  // extra frames of clearance per consecutive failure
		constexpr int maxBacktrackGap   = 600; // cap so we don't eventually rewind the whole level every time
		constexpr int maxFailsAtNode    = 5;   // after this many failed retries, stop reusing this node

		stuckStreak++;
		int dynamicGap = std::min(minBacktrackGap + (stuckStreak - 1) * gapGrowthPerFail, maxBacktrackGap);

		int deathFrame = lvl.currentFrame();
		CheckpointNode* target = nullptr;

		for (int i = static_cast<int>(checkpoints.size()) - 1; i >= 0; --i) {
			auto& cp = checkpoints[i];
			if (cp.frame > deathFrame - dynamicGap) continue; // too close, no room to react differently
			target = &cp;
			if (cp.failCount < maxFailsAtNode) break; // still usable — stop here
			// else: this node is worn out, keep walking further back
		}

		if (target) {
			target->failCount++;
			lvl.gameStates.resize(target->frame);
			lvl.gameStates.back() = target->state;
			lvl.press = target->state.button;

			// Anything catalogued ahead of where we just rewound to belonged
			// to the branch we're abandoning — throw it out, since jumping to
			// it later would leave a gap of uninitialized states behind it.
			checkpoints.erase(
				std::remove_if(checkpoints.begin(), checkpoints.end(),
					[&](CheckpointNode const& c) { return c.frame > target->frame; }),
				checkpoints.end());
			lastCatalogedPct = pctOf(target->x);
		} else if (lvlBest.currentFrame() > 1) {
			// Our local catalog is exhausted — the required gap has grown
			// past our entire recorded history. Fall back to the best point
			// this branch has ever reached rather than restarting from
			// frame 1 every time.
			resetTo(lvlBest);
		} else {
			// Truly nothing to fall back on yet.
			lvl.rollback(1);
			lvl.press = lvl.gameStates.back().button;
		}
	}

	void recordProgress() {
		stuckStreak = 0;
		iterations = 100;
		catalogAndPrune();
		if (lvl.currentFrame() > lvlBest.currentFrame()) {
			lvlBest = lvl;
		}
	}
};

} // namespace

PathfindResult pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback) {
	PathfindResult result;
	Level2 seed(lvlString);

	// Biased sampling fallback: if no objects found, sample uniformly.
	// Shared read-only across every branch — never mutated after this point.
	std::vector<int> interesting = seed.interestingFrames;
	if (interesting.empty()) {
		int maxF = std::max(100, static_cast<int>((seed.length / 400.0f) * 240.0f));
		for (int f = 1; f < maxF; f += 10) {
			interesting.push_back(f);
		}
	}

	// Run several independent searches in parallel instead of one. Leave one
	// core free for the game itself (this mod runs alongside GD, not instead
	// of it), and cap at 4 — more than that gives diminishing returns for how
	// cheap each individual simulated frame is.
	unsigned int hc = std::thread::hardware_concurrency();
	int numBranches = hc == 0 ? 2 : static_cast<int>(std::clamp(hc - 1, 1u, 4u));

	std::random_device rd;
	std::vector<SearchBranch> branches;
	branches.reserve(numBranches);
	for (int b = 0; b < numBranches; ++b) {
		branches.emplace_back(seed, rd());
	}

	// The current best-progressed state across ALL branches. Acts as a
	// shared "best-ever-reached" archive: stragglers periodically get pulled
	// up to it instead of continuing to dig at an inferior line, and it's
	// what actually gets exported at the end.
	std::mutex leaderMtx;
	Level2 leader = seed;
	std::atomic<double> globalProgress{0.0};

	constexpr int   syncEveryPasses    = 25;  // how often a branch checks in against the others
	constexpr float behindPctToResync  = 3.0f; // how far behind the leader before a branch gives up and joins it
	constexpr int   pairsPerTry        = 8;   // 8 pairs = 16 click events, more effective than 15 singles

	auto runBranch = [&](SearchBranch& branch) {
		std::uniform_int_distribution<int> idxDist(0, static_cast<int>(interesting.size()) - 1);
		std::uniform_int_distribution<int> jitterDist(-10, 10);
		std::uniform_int_distribution<int> holdDist(2, 25); // hold durations from 2 to 25 frames

		int passesSinceSync = 0;

		while (!stop && branch.lvl.gameStates.back().pos.x < branch.lvl.length) {
			auto frame = branch.lvl.currentFrame();
			constexpr int maxSimFrames = 1500;
			std::vector<uint8_t> bestInputs(maxSimFrames, 0);
			int bestFrame = frame;

			// Find the range of interesting frames near our current position
			auto it = std::lower_bound(interesting.begin(), interesting.end(), frame);
			int startIdx = static_cast<int>(it - interesting.begin());
			auto horizon = std::lower_bound(interesting.begin() + startIdx, interesting.end(), frame + maxSimFrames);
			int horizonEnd = static_cast<int>(horizon - interesting.begin());
			int availableRange = std::max(1, horizonEnd - startIdx);

			for (int i = 0; i < branch.iterations; ++i) {
				std::vector<uint8_t> inputs(maxSimFrames, 0);

				// Generate click PAIRS (press + release) instead of single frames
				for (int j = 0; j < pairsPerTry; ++j) {
					int idx = startIdx + (idxDist(branch.rng) % availableRange);
					if (idx >= static_cast<int>(interesting.size()))
						idx = static_cast<int>(interesting.size()) - 1;

					int pressFrame = interesting[idx] + jitterDist(branch.rng);
					if (pressFrame >= frame && pressFrame < frame + maxSimFrames) {
						inputs[pressFrame - frame] = 1;

						int releaseFrame = pressFrame + holdDist(branch.rng);
						if (releaseFrame < frame + maxSimFrames) {
							inputs[releaseFrame - frame] = 1;
						}
					}
				}

				int nf = tryInputs(branch.lvl, inputs, bestFrame);
				if (nf > bestFrame) {
					bestFrame = nf;
					bestInputs = inputs;
					if (bestFrame - frame > 500)
						break;
				}
			}

			if (bestFrame == frame) {
				branch.handleStuck();
				branch.iterations = std::min(branch.iterations + 30, 250); // ramp up when stuck
			} else {
				// Advance 2/3 of the best distance found
				int advanceTo = bestFrame - (bestFrame - frame) / 3;
				for (int i = frame; i < advanceTo; ++i) {
					if (i >= frame && i - frame < static_cast<int>(bestInputs.size()) && bestInputs[i - frame]) {
						branch.lvl.press = !branch.lvl.press;
					}
					branch.lvl.runFrame(branch.lvl.press);
				}
				branch.recordProgress();
			}

			// --- Cross-branch sync ---
			// Either report a new global best, or — if we've fallen well
			// behind whoever currently has one — abandon our own line and
			// continue from theirs instead.
			if (++passesSinceSync >= syncEveryPasses) {
				passesSinceSync = 0;
				std::lock_guard<std::mutex> lock(leaderMtx);
				if (branch.lvlBest.currentFrame() > leader.currentFrame()) {
					leader = branch.lvlBest;
				} else {
					float leaderPct = branch.pctOf(leader.latestState().pos.x);
					float ownPct = branch.pctOf(branch.lvlBest.latestState().pos.x);
					if (leaderPct - ownPct > behindPctToResync) {
						branch.resetTo(leader);
					}
				}
			}

			double p = std::min((branch.lvl.latestState().pos.x / branch.lvl.length) * 100.0f, 100.0f);
			double prev = globalProgress.load();
			while (p > prev && !globalProgress.compare_exchange_weak(prev, p)) {}
			if (callback) callback(globalProgress.load());
		}

		// Final check-in in case this branch never got to sync mid-run
		// (e.g. it finished the level before the interval came up).
		std::lock_guard<std::mutex> lock(leaderMtx);
		if (branch.lvlBest.currentFrame() > leader.currentFrame()) {
			leader = branch.lvlBest;
		}
	};

	std::vector<std::thread> threads;
	threads.reserve(numBranches);
	for (auto& branch : branches) {
		threads.emplace_back(runBranch, std::ref(branch));
	}
	for (auto& t : threads) t.join();

	Level2& lvlBest = leader;

	Replay2 output;
	bool lastButton = false;
	for (auto const& st : lvlBest.gameStates) {
		if (st.frame >= 1 && st.button != lastButton) {
			output.inputs.push_back(gdr::Input(static_cast<uint32_t>(st.frame), 1, false, st.button));
			lastButton = st.button;
		}
	}
	if (lastButton && !lvlBest.gameStates.empty()) {
		output.inputs.push_back(gdr::Input(static_cast<uint32_t>(lvlBest.gameStates.back().frame + 1), 1, false, false));
	}
	result.replay = output.exportData().unwrapOr({});
	result.progress = seed.length > 0.0f
		? std::min((lvlBest.latestState().pos.x / lvlBest.length) * 100.0, 100.0)
		: 0.0;
	result.solved = !lvlBest.latestState().dead && lvlBest.latestState().pos.x >= lvlBest.length;
	return result;
}
