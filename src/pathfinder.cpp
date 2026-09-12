#include <set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <chrono>
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

// Dual mode is not simulateable with the single Player in the sim: in real GD
// both cubes must survive, but the sim only tracks one. Detect dual portals
// (object id 286) up front so the mod refuses loudly instead of exporting a
// macro that is guaranteed to desync on the second player.
static bool levelHasDual(std::string const& lvlString) {
	std::stringstream ss(lvlString);
	std::string obj;
	while (std::getline(ss, obj, ';')) {
		std::stringstream o(obj);
		std::string k, v;
		while (std::getline(o, k, ',')) {
			std::getline(o, v, ',');
			if (k == "1" && v == "286") return true;
		}
	}
	return false;
}

// Count gameplay-affecting effect triggers (Move/Rotate/Follow/Spawn/etc.).
// These are the 2.0/2.1 "effect" objects (object ids 1049-1072) plus the
// pre-2.1 instant Rotate (360). The sim is static — it drops these objects
// entirely — but in real GD they move or rotate geometry mid-run, so a macro
// that solves the simulated level can desync from how the level actually
// plays. Cosmetic color/alpha triggers (899, 900, 901, 915, 1006, 1007) are
// deliberately excluded: they can't move geometry and are present on nearly
// every decorated level, so flagging them would just be noise.
static int countLevelTriggers(std::string const& lvlString) {
	int count = 0;
	size_t i = 0;
	while (i < lvlString.size()) {
		size_t semi = lvlString.find(';', i);
		if (semi == std::string::npos) break;
		std::string obj = lvlString.substr(i, semi - i);
		i = semi + 1;
		size_t coma = obj.find(',');
		if (coma == std::string::npos || coma == 0) continue;
		long id = std::strtol(obj.substr(0, coma).c_str(), nullptr, 10);
		if (id == 360 || (id >= 1049 && id <= 1072)) ++count;
	}
	return count;
}

struct Level2 : public Level {
	bool press = false;
	float highestY = 0;
	bool dualLevel = false;
	std::vector<int> interestingFrames; // pre-computed frames near objects

	using Level::Level;

	Level2(std::string const& lvlString) : Level(lvlString) {
		// Find highest y
		for (auto& i : sections) {
			for (auto& j : i) {
				highestY = std::max(highestY, j->pos.y);
			}
		}
		dualLevel = levelHasDual(lvlString);
		// Pre-compute interesting frames near objects
		buildInterestingFrames();
	}

	// Upper death boundary for the search. The sim already kills the player at
	// PHYS_DEATH_Y_HIGH (calibrated to GD); the old hard-coded 1300 in
	// tryInputs wrongly rejected perfectly valid high routes in tall levels,
	// so allow levels with structures higher than that a little extra room.
	float deathUpperBound() const {
		return std::max(PHYS_DEATH_Y_HIGH, highestY + 300.0f);
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

	int tryInputs(Level2& lvl, std::vector<uint8_t> const& inputs, int currentBestFrame, std::atomic<long>& framesSimulated) {
	auto frame = lvl.currentFrame();
	auto press_before = lvl.press;

	// Save minimal state for fast restore instead of full rollback
	int savedStateCount = static_cast<int>(lvl.gameStates.size());
	auto savedPlayer = lvl.gameStates.back();

	constexpr int maxSimFrames = 1500;
	constexpr int stuckThreshold = 60;     // frames without progress = stuck
		int f = frame;
	int endFrame = frame + maxSimFrames;

	float lastProgressX = lvl.latestState().pos.x;
	float lastProgressY = lvl.latestState().pos.y;
	int framesSinceProgress = 0;

	while (!lvl.gameStates.back().dead && f < endFrame) {
			if (f >= frame && f - frame < static_cast<int>(inputs.size()) && inputs[f - frame]) {
			lvl.press = !lvl.press;
		}
		lvl.runFrame(lvl.press);
		++framesSimulated;
		++f;

		// --- Early termination checks ---

		// Stuck detection: no meaningful motion for too long. Vertical travel
		// counts as progress too — ship/UFO climbs, elevators and drop
		// sections barely move X yet are perfectly viable, and the old
		// X-only check treated them as stuck and forced constant backtracks.
		float currentX = lvl.latestState().pos.x;
		float currentY = lvl.latestState().pos.y;
		if (currentX > lastProgressX + 0.5f || std::abs(currentY - lastProgressY) > 1.5f) {
			lastProgressX = currentX;
			lastProgressY = currentY;
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

	// Reject states that left the play area. The sim itself marks death at
	// PHYS_DEATH_Y_HIGH, so this is just a safety net; the search must never
	// reject a route the sim considers alive.
	if (lastY > lvl.deathUpperBound() || lastY < 0.0f)
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
	// Inputs scheduled past the current commit point; plan[t] toggles press
	// state at absolute frame (commit frame + t). Carried between passes so a
	// surviving plan is exploited and refined instead of being re-randomized
	// from scratch every time.
	std::vector<uint8_t> plan;

	// Explorer branches intentionally diverge: they never resync to the global
	// leader and they sample wider, so the search keeps covering alternative
	// lines instead of converging on whatever the front-runner happens to be.
	bool explorer = false;

	SearchBranch(Level2 const& seed, uint32_t seedVal, bool explorer)
		: lvl(seed), lvlBest(seed), rng(seedVal), explorer(explorer) {
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
		plan.clear();
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
		plan.clear(); // the carried plan would just drive into the same death
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

PathfindResult pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback, int inputOffset, int solverSeed) {
	PathfindResult result;
	Level2 seed(lvlString);

	auto wallStart = std::chrono::steady_clock::now();

	if (seed.dualLevel) {
		result.error = "Dual mode is not supported by the simulation (yet) — pick a non-dual level.";
		return result;
	}

	// Levels with Move/Rotate/Follow/Spawn effect triggers (A1) can't be
	// modelled by the static sim, so flag them up front: even a "solved"
	// simulated run may desync from the real level.
	int triggerCount = countLevelTriggers(lvlString);
	if (triggerCount > 0) {
		result.warning = "Level contains " + std::to_string(triggerCount) +
			" unsupported trigger object(s) (e.g. Move/Rotate) — they move geometry in-game but not in the sim, so the macro may desync.";
	}

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
	// solverSeed == 0 means "random"; any other value makes the whole search
	// reproducible (same level + same seed = same search). Branch rng streams
	// are derived from a single base seed so a fixed setting fully pins the
	// run.
	uint32_t baseSeed = solverSeed == 0 ? rd() : static_cast<uint32_t>(solverSeed);
	result.seedUsed = baseSeed;
	std::vector<SearchBranch> branches;
	branches.reserve(numBranches);
	for (int b = 0; b < numBranches; ++b) {
		branches.emplace_back(seed, baseSeed + static_cast<uint32_t>(b), b % 2 == 1);
	}

	// The current best-progressed state across ALL branches. Acts as a
	// shared "best-ever-reached" archive: stragglers periodically get pulled
	// up to it instead of continuing to dig at an inferior line, and it's
	// what actually gets exported at the end.
	std::mutex leaderMtx;
	Level2 leader = seed;
	// Forward input plan of whoever currently holds the lead, plus the
	// absolute frame it is aligned to. Stragglers that resync to the leader
	// adopt this plan so they skip re-searching territory the leader already
	// covered.
	std::vector<uint8_t> leaderPlan;
	int leaderPlanBase = 0;
	std::atomic<double> globalProgress{0.0};
	std::atomic<long> framesSimulated{0};

	constexpr int   syncEveryPasses    = 25;  // how often a branch checks in against the others
	constexpr float behindPctToResync  = 3.0f; // how far behind the leader before a branch gives up and joins it
	constexpr int   basePairsPerTry    = 8;   // 8 pairs = 16 click events, more effective than 15 singles

	auto runBranch = [&](SearchBranch& branch) {
		std::uniform_int_distribution<int> idxDist(0, static_cast<int>(interesting.size()) - 1);

		int passesSinceSync = 0;

		while (!stop && branch.lvl.gameStates.back().pos.x < branch.lvl.length) {
			auto frame = branch.lvl.currentFrame();
			constexpr int maxSimFrames = 1500;

			// Per-pass input sampling ranges. Vehicles react differently to
			// mis-timed clicks: Wave needs tight taps near the obstacle
			// (autofire-state can't tolerate long holds), while ship/UFO/
			// swing give a strafe window that tolerates wider jitter and
			// longer holds. Explorer branches sample even wider so they keep
			// inventing alternative lines instead of converging on the leader.
			std::uniform_int_distribution<int> jitterDist(-10, 10);
			std::uniform_int_distribution<int> holdDist(2, 25);
			int pairsPerTry = basePairsPerTry;
			switch (branch.lvl.latestState().vehicle.type) {
				case VehicleType::Wave:
					jitterDist = std::uniform_int_distribution<int>(-3, 3);
					holdDist = std::uniform_int_distribution<int>(2, 6);
					break;
				case VehicleType::Ship:
				case VehicleType::Ufo:
				case VehicleType::Swing:
					jitterDist = std::uniform_int_distribution<int>(-6, 6);
					holdDist = std::uniform_int_distribution<int>(2, 12);
					break;
				default:
					break;
			}
			if (branch.explorer) {
				pairsPerTry = 12;
				if (branch.lvl.latestState().vehicle.type != VehicleType::Wave) {
					jitterDist = std::uniform_int_distribution<int>(-15, 15);
					holdDist = std::uniform_int_distribution<int>(2, 30);
				}
			}

			std::vector<uint8_t> bestInputs(maxSimFrames, 0);
			int bestFrame = frame;

			int carryLen = std::min(static_cast<int>(branch.plan.size()), static_cast<int>(maxSimFrames));

			// Find the range of interesting frames near our current position
			auto it = std::lower_bound(interesting.begin(), interesting.end(), frame);
			int startIdx = static_cast<int>(it - interesting.begin());
			auto horizon = std::lower_bound(interesting.begin() + startIdx, interesting.end(), frame + maxSimFrames);
			int horizonEnd = static_cast<int>(horizon - interesting.begin());
			int availableRange = std::max(1, horizonEnd - startIdx);

			for (int i = 0; i < branch.iterations; ++i) {
				std::vector<uint8_t> inputs(maxSimFrames, 0);

				// 1) Exploit the surviving plan: its future clicks are carried
				//    verbatim so a good run isn't re-randomized every pass.
				for (int t = 0; t < carryLen; ++t)
					inputs[t] = branch.plan[t];

				// 2) On later iterations, re-roll the final few plan frames so
				//    the commit boundary can be re-timed by a frame or two.
				if (i > 0 && carryLen >= 2) {
					int tune = std::min(8, carryLen);
					int lo = carryLen - tune;
					for (int t = lo; t < carryLen; ++t) inputs[t] = 0;
					for (int j = 0; j < 2; ++j) {
						int rel = lo + (jitterDist(branch.rng) + tune / 2);
						if (rel < 0) rel = 0;
						if (rel < maxSimFrames - 1) {
							inputs[rel] = 1;
							int rel2 = rel + holdDist(branch.rng);
							if (rel2 < maxSimFrames) inputs[rel2] = 1;
						}
					}
				}

				// 3) Fresh probes are only placed beyond the carried plan so
				//    they extend it rather than corrupt it.
				for (int j = 0; j < pairsPerTry; ++j) {
					int idx = startIdx + (idxDist(branch.rng) % availableRange);
					if (idx >= static_cast<int>(interesting.size()))
						idx = static_cast<int>(interesting.size()) - 1;

					int rel = interesting[idx] + jitterDist(branch.rng) - frame;
					if (rel < carryLen || rel >= maxSimFrames) continue;
					inputs[rel] = 1;

					int rel2 = interesting[idx] + jitterDist(branch.rng) + holdDist(branch.rng) - frame;
					if (rel2 >= maxSimFrames) continue;
					inputs[rel2] = 1;
				}

				int nf = tryInputs(branch.lvl, inputs, bestFrame, framesSimulated);
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
				++framesSimulated;
			}
				// Carry the winning plan's future clicks into the next pass so
				// they're now scheduled relative to the new commit point.
				int keepFrom = advanceTo - frame;
				if (keepFrom >= maxSimFrames) {
					branch.plan.clear();
				} else {
					branch.plan.assign(bestInputs.begin() + keepFrom, bestInputs.begin() + maxSimFrames);
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
					// Publish the forward plan so other branches can pick up
					// where we left off instead of restarting the search. The
					// plan is aligned to the current commit point, which equals
					// lvlBest.currentFrame() whenever we're promoting a fresh
					// best.
					leaderPlan = branch.plan;
					leaderPlanBase = branch.lvl.currentFrame();
				} else if (!branch.explorer) {
					// Explorers (B3) never adopt the leader's line — that's the
					// whole point of them. Everyone else, if we've fallen well
					// behind whoever currently has a best — abandon our own line
					// and continue from theirs instead.
					float leaderPct = branch.pctOf(leader.latestState().pos.x);
					float ownPct = branch.pctOf(branch.lvlBest.latestState().pos.x);
					if (leaderPct - ownPct > behindPctToResync) {
						int resyncFrame = leader.currentFrame();
						branch.resetTo(leader);
						// Adopt the leader's plan, re-aligned to our frame.
						int shift = resyncFrame - leaderPlanBase;
						branch.plan.clear();
						if (shift >= 0 && shift < static_cast<int>(leaderPlan.size())) {
							branch.plan.assign(leaderPlan.begin() + shift, leaderPlan.end());
						} else if (shift < 0) {
							branch.plan.assign(static_cast<size_t>(-shift), 0);
							branch.plan.insert(branch.plan.end(), leaderPlan.begin(), leaderPlan.end());
						}
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
			leaderPlan = branch.plan;
			leaderPlanBase = branch.lvl.currentFrame();
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
	// How many 240Hz frames to shift every exported input by. Lets users
	// compensate for whatever offset their replay bot/bus timing introduces.
	int off = inputOffset;
	int deathFrame = static_cast<int>(lvlBest.gameStates.size());
	float minMargin = 0.0f;
	for (auto const& st : lvlBest.gameStates) {
		if (st.frame >= 1 && st.button != lastButton) {
			int gap = deathFrame - st.frame;
			if (minMargin == 0.0f) minMargin = static_cast<float>(gap);
			else minMargin = std::min(minMargin, static_cast<float>(gap));

			int f = st.frame + off;
			if (f < 1) f = 1;
			output.inputs.push_back(gdr::Input(static_cast<uint32_t>(f), 1, false, st.button));
			lastButton = st.button;
		}
	}
	if (lastButton && !lvlBest.gameStates.empty()) {
		int f = lvlBest.gameStates.back().frame + 1 + off;
		if (f < 1) f = 1;
		output.inputs.push_back(gdr::Input(static_cast<uint32_t>(f), 1, false, false));
	}
	result.replay = output.exportData().unwrapOr({});
	result.progress = seed.length > 0.0f
		? std::min((lvlBest.latestState().pos.x / lvlBest.length) * 100.0, 100.0)
		: 0.0;
	result.solved = !lvlBest.latestState().dead && lvlBest.latestState().pos.x >= lvlBest.length;
	// Only meaningful when the run ended in a death: the tightest gap between
	// an input and the hazard that killed the run. Solved runs get 0.
	result.minMargin = lvlBest.latestState().dead ? minMargin : 0.0f;

	// Solver diagnostics (C7).
	result.wallTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - wallStart).count();
	result.framesSimulated = framesSimulated.load();
	result.branchesUsed = numBranches;

	// Round-trip self-check (C6): re-import the exported .gdr2 and replay it
	// from a fresh sim, exactly the way the mod itself verifies macros in
	// debug.cpp (some-frame-offset: input at frame F sets the hold state for
	// the runFrame at F). The export applies `inputOffset` to every frame, so
	// un-shift before replaying to make sure the raw macro really reproduces
	// the simulated solution independent of any bot timing compensation.
	if (!result.replay.empty() && result.error.empty()) {
		auto rr = Replay2::importData(result.replay);
		if (rr.isErr()) {
			result.error = "Round-trip self-check failed: could not re-import the exported macro.";
		} else {
			auto inputs = rr.unwrap().inputs;
			for (auto& in : inputs) {
				int unshifted = static_cast<int>(in.frame) - off;
				in.frame = static_cast<uint32_t>(std::max(unshifted, 1));
			}

			std::string encoded = "0";
			bool currentHold = false;
			int maxFrame = inputs.empty() ? 1 : static_cast<int>(inputs.back().frame);
			for (int i = 1; i < maxFrame; ++i) {
				if (!inputs.empty() && i == static_cast<int>(inputs.front().frame)) {
					currentHold = inputs.front().down;
					inputs.erase(inputs.begin());
				}
				encoded += currentHold ? '1' : '0';
			}

			Level2 fresh(lvlString);
			for (size_t i = 2; i < encoded.size() && !fresh.latestState().dead; ++i) {
				fresh.runFrame(encoded[i] == '1');
			}

			int actualFrame = fresh.currentFrame();
			int expectedFrame = lvlBest.currentFrame();
			// Allow a couple frames of slack for the mod's inherent ±1
			// input-framing convention (the input-offset setting exists
			// because of it); a genuine export bug desyncs by far more.
			bool reproduced = std::abs(actualFrame - expectedFrame) <= 2 &&
				std::fabs(fresh.latestState().pos.x - lvlBest.latestState().pos.x) < 6.0f;
			if (!reproduced) {
				result.error = "Round-trip self-check failed: the exported macro does not reproduce the solved run "
					"(replay ended at frame " + std::to_string(actualFrame) +
					", expected " + std::to_string(expectedFrame) + ").";
			}
		}
	}

	return result;
}
