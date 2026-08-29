#include <set>
#include <bitset>
#include <algorithm>
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
		float baseSpeed = PHYS_SPEEDS[std::clamp(gameStates[0].speed, 0, 4)];

		std::set<int> frameSet; // avoid duplicates during building

		for (auto& section : sections) {
			for (auto& obj : section) {
				// Estimate which frame the player will encounter this object
				// frame ≈ (object_x / speed) * fps
				float encounterFrame = (obj->pos.x / baseSpeed) * fps;
				int startFrame = static_cast<int>(encounterFrame - windowRadius);
				int endFrame = static_cast<int>(encounterFrame + windowRadius);

				if (startFrame < 1) startFrame = 1;
				for (int f = startFrame; f <= endFrame; ++f) {
					frameSet.insert(f);
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

int tryInputs(Level2& lvl, std::bitset<65536> const& inputs, int currentBestFrame) {
	auto frame = lvl.currentFrame();
	auto press_before = lvl.press;

	// Save minimal state for fast restore instead of full rollback
	int savedStateCount = static_cast<int>(lvl.gameStates.size());
	auto savedPlayer = lvl.gameStates.back();

	constexpr int maxSimFrames = 1500;
	constexpr int stuckThreshold = 60;     // frames without forward progress = stuck
	constexpr int earlyCheckFrame = 400;   // check relative performance after this many frames
	constexpr int earlyKillMargin = 150;   // if behind by this much at check, kill

	int f = frame;
	int endFrame = frame + maxSimFrames;

	float lastProgressX = lvl.latestState().pos.x;
	int framesSinceProgress = 0;

	while (!lvl.gameStates.back().dead && f < endFrame) {
		if (inputs.test(f)) {
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

std::vector<uint8_t> pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback) {
	Level2 lvl(lvlString);
	std::random_device rd;
	std::mt19937 rng(rd());

	// Biased sampling fallback: if no objects found, sample uniformly
	std::vector<int> interesting = lvl.interestingFrames;
	if (interesting.empty()) {
		int maxF = std::max(100, static_cast<int>((lvl.length / 400.0f) * 240.0f));
		for (int f = 1; f < maxF; f += 10) {
			interesting.push_back(f);
		}
	}
	std::uniform_int_distribution<int> idxDist(0, static_cast<int>(interesting.size()) - 1);
	std::uniform_int_distribution<int> jitterDist(-10, 10);

	// For click pair generation: hold durations from 2 to 25 frames
	std::uniform_int_distribution<int> holdDist(2, 25);

	// --- Living-checkpoint catalog ---
	// Every time the sim survives to a new % of the level, its full state gets
	// catalogued here. When a branch dies (or gets stuck making no progress),
	// we rewind to the nearest catalogued checkpoint instead of an arbitrary
	// frame count back — close enough that we're not re-solving the whole
	// level from scratch, but far enough before the death point that a
	// different click actually has room to change the outcome instead of
	// dying the exact same way again.
	struct CheckpointNode {
		int frame;
		float x;
		Player state;
		int failCount = 0; // retries from this exact node without escalating further back
	};

	constexpr float catalogStepPct   = 1.0f;  // catalogue roughly every 1% of level progress
	constexpr float catalogWindowPct = 10.0f; // GC anything more than ~10% behind our current best
	constexpr int   minBacktrackGap  = 40;    // frames of clearance a checkpoint needs before a death (room for a press+release)
	constexpr int   maxFailsAtNode   = 5;     // after this many failed retries, stop reusing this node and go further back

	auto pctOf = [&](float x) {
		return lvl.length > 0.0f ? (x / lvl.length) * 100.0f : 0.0f;
	};

	std::vector<CheckpointNode> checkpoints;
	checkpoints.push_back({lvl.currentFrame(), lvl.latestState().pos.x, lvl.latestState()});
	float lastCatalogedPct = pctOf(lvl.latestState().pos.x);

	auto catalogAndPrune = [&]() {
		float pct = pctOf(lvl.latestState().pos.x);
		if (pct - lastCatalogedPct < catalogStepPct) return;

		checkpoints.push_back({lvl.currentFrame(), lvl.latestState().pos.x, lvl.latestState()});
		lastCatalogedPct = pct;

		// Drop anything that's fallen too far behind our current progress —
		// e.g. once we're at 17%, the 1-9% checkpoints no longer help us
		// backtrack usefully. Index 0 is always kept as a last-resort anchor
		// back to the very start of the level.
		float cutoffPct = pct - catalogWindowPct;
		if (checkpoints.size() > 1) {
			checkpoints.erase(
				std::remove_if(checkpoints.begin() + 1, checkpoints.end() - 1,
					[&](CheckpointNode const& c) { return pctOf(c.x) < cutoffPct; }),
				checkpoints.end() - 1);
		}
	};

	Level2 lvlBest = lvl;

	// Progressive iteration count: start low, increase only when stuck
	int iterations = 100;
	constexpr int pairsPerTry = 8; // 8 pairs = 16 click events, more effective than 15 singles

	while (lvl.gameStates.back().pos.x < lvl.length) {
		auto frame = lvl.currentFrame();
		std::bitset<65536> bestInputs;
		int bestFrame = frame;

		// Find the range of interesting frames near our current position
		auto it = std::lower_bound(interesting.begin(), interesting.end(), frame);
		int startIdx = static_cast<int>(it - interesting.begin());
		int availableRange = std::max(1, static_cast<int>(interesting.size() - startIdx));

		for (int i = 0; i < iterations; ++i) {
			std::bitset<65536> inputs;

			// Generate click PAIRS (press + release) instead of single frames
			for (int j = 0; j < pairsPerTry; ++j) {
				// Pick a nearby interesting frame as the PRESS point
				int idx = startIdx + (idxDist(rng) % availableRange);
				if (idx >= static_cast<int>(interesting.size()))
					idx = static_cast<int>(interesting.size()) - 1;

				int pressFrame = interesting[idx] + jitterDist(rng);
				if (pressFrame >= frame && pressFrame < 65534) {
					inputs.set(static_cast<uint16_t>(pressFrame));

					// Set the RELEASE frame after a random hold duration
					int releaseFrame = pressFrame + holdDist(rng);
					if (releaseFrame < 65535) {
						inputs.set(static_cast<uint16_t>(releaseFrame));
					}
				}
			}

			int nf = tryInputs(lvl, inputs, bestFrame);
			if (nf > bestFrame) {
				bestFrame = nf;
				bestInputs = inputs;
				// Early exit if we found great progress
				if (bestFrame - frame > 500)
					break;
			}
		}

		if (bestFrame == frame) {
			// No progress this pass — rewind to the nearest catalogued
			// checkpoint that's far enough back to give a different click
			// room to matter, instead of landing right back at the same death.
			int deathFrame = frame;
			CheckpointNode* target = nullptr;

			for (int i = static_cast<int>(checkpoints.size()) - 1; i >= 0; --i) {
				auto& cp = checkpoints[i];
				if (cp.frame > deathFrame - minBacktrackGap) continue; // too close, no room to react differently
				target = &cp;
				if (cp.failCount < maxFailsAtNode) break; // still usable — stop here
				// else: this node is worn out, keep walking further back
			}

			if (target) {
				target->failCount++;
				lvl.gameStates.resize(target->frame);
				lvl.gameStates.back() = target->state;
				lvl.press = target->state.button;

				// Anything catalogued ahead of where we just rewound to
				// belonged to the branch we're abandoning — throw it out,
				// since jumping to it later would leave a gap of uninitialized
				// states in gameStates.
				checkpoints.erase(
					std::remove_if(checkpoints.begin(), checkpoints.end(),
						[&](CheckpointNode const& c) { return c.frame > target->frame; }),
					checkpoints.end());
				lastCatalogedPct = pctOf(target->x);
			} else {
				// Nothing catalogued far back enough yet (very early in the level)
				lvl.rollback(1);
				lvl.press = lvl.gameStates.back().button;
			}

			iterations = std::min(iterations + 30, 250); // ramp up when stuck
		} else {
			// Progress made — reset to lower iterations
			iterations = 100;

			// Advance 2/3 of the best distance found
			int advanceTo = bestFrame - (bestFrame - frame) / 3;
			for (int i = frame; i < advanceTo; ++i) {
				if (bestInputs.test(i)) {
					lvl.press = !lvl.press;
				}
				lvl.runFrame(lvl.press);
			}

			catalogAndPrune();
		}

		if (lvl.currentFrame() > lvlBest.currentFrame()) {
			lvlBest = lvl;
		}
		if (callback)
			callback(std::min((lvl.latestState().pos.x / lvl.length) * 100, 100.0f));

		if (stop)
			break;
	}

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
	return output.exportData().unwrapOr({});
}
