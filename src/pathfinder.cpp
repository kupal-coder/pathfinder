#include <set>
#include <bitset>
#include <algorithm>
#include <Level.hpp>
#include <random>
#include <gdr/gdr.hpp>
#include "pathfinder.hpp"

class Replay2 : public gdr::Replay<Replay2, gdr::Input<"">> {
 public:
	Replay2() : Replay("Pathfinder", 1){}
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
		float baseSpeed = player_speeds[std::clamp(gameStates[0].speed, 0, 4)];

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

		// 2. Relative performance: if clearly behind the best path, give up early
		if (currentBestFrame > frame + earlyCheckFrame && f == frame + earlyCheckFrame) {
			int framesSimulated = f - frame;
			int bestProgress = currentBestFrame - frame;
			if (framesSimulated < bestProgress - earlyKillMargin) {
				break; // this path is way behind, no need to continue
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

	// Distribution for biased sampling: pick from interesting frames
	auto const& interesting = lvl.interestingFrames;
	if (interesting.empty()) {
		return {};
	}
	std::uniform_int_distribution<int> idxDist(0, static_cast<int>(interesting.size()) - 1);
	std::uniform_int_distribution<int> jitterDist(-10, 10);

	// For click pair generation: hold durations from 2 to 25 frames
	std::uniform_int_distribution<int> holdDist(2, 25);

	int trueBest = 0;
	int fail = 1;
	int numAway = 1000;
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
				if (bestFrame - frame > 500 && fail < 1000)
					break;
			}
		}

		if (bestFrame == frame) {
			// No progress — roll back and increase iterations for next time
			lvl.rollback(std::max(std::max(frame - fail, trueBest - numAway), 1));
			fail += 5;
			iterations = std::min(iterations + 30, 250); // ramp up when stuck

			if (fail > numAway + 1000) {
				numAway += 1000;
				fail = 1;
				if (numAway > 10000) {
					numAway = 1000;
					trueBest = 0;
					lvl.rollback(1);
				}
			} else if (fail > 100) {
				fail += 50;
			}
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
		}

		if (lvl.currentFrame() > trueBest) {
			trueBest = lvl.currentFrame();
			fail = 0;
			numAway = 1000;
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
	for (auto& i : lvlBest.gameStates) {
	    if (i.frame > 1 && i.button != i.prevPlayer().button)
	        output.inputs.push_back(gdr::Input(i.frame, 1, false, i.button));
	}
	return output.exportData().unwrapOr({});
}
