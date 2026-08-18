#include <set>
#include <algorithm>
#include <limits>
#include <Level.hpp>
#include <Physics.hpp>
#include <Portals.hpp>
#include <random>
#include <gdr/gdr.hpp>
#include "pathfinder.hpp"

class Replay2 : public gdr::Replay<Replay2, gdr::Input<"">> {
 public:
	Replay2() : Replay("Pathfinder", 1) {}
};

struct Level2 : public Level {
	bool press = false;
	float highestY = 0.0f;
	std::vector<int> interestingFrames;

	using Level::Level;

	Level2(std::string const& lvlString) : Level(lvlString) {
		for (auto const& section : sections) {
			for (auto const& object : section)
				highestY = std::max(highestY, object->pos.y);
		}
	}

	/**
	 * Predict object encounter frames from the current state. Walking objects in X order
	 * lets the prediction account for Start Positions and deterministic speed portals.
	 */
	void buildInterestingFrames() {
		constexpr int windowRadius = 30;
		interestingFrames.clear();

		std::vector<Object*> objects;
		objects.reserve(objectCount);
		for (auto& section : sections) {
			for (auto& object : section)
				objects.push_back(object.operator->());
		}
		std::sort(objects.begin(), objects.end(), [](Object const* a, Object const* b) {
			return a->pos.x < b->pos.x;
		});

		float cursorX = latestState().pos.x;
		double predictedFrame = static_cast<double>(currentFrame());
		int predictedSpeed = std::clamp(latestState().speed, 0, 4);
		std::set<int> frames;

		for (auto* object : objects) {
			if (object->pos.x + object->size.x * 0.5f < cursorX)
				continue;

			float distance = std::max(0.0f, object->pos.x - cursorX);
			predictedFrame += static_cast<double>(distance / PHYS_SPEEDS[predictedSpeed]) * PHYS_FPS;
			cursorX = std::max(cursorX, object->pos.x);

			if (predictedFrame <= static_cast<double>(std::numeric_limits<int>::max() - windowRadius)) {
				int encounter = static_cast<int>(predictedFrame);
				int start = std::max(currentFrame(), encounter - windowRadius);
				for (int frame = start; frame <= encounter + windowRadius; ++frame)
					frames.insert(frame);
			}

			if (auto const* portal = object->asSpeedPortal())
				predictedSpeed = std::clamp(portal->speed, 0, 4);
		}

		interestingFrames.assign(frames.begin(), frames.end());
	}
};

bool isLevelEnd(Level2 const& lvl) {
	return lvl.gameStates.back().pos.x >= lvl.length;
}

int tryInputs(Level2& lvl, std::set<int> const& inputs, std::atomic_bool const& stop) {
	auto frame = lvl.currentFrame();
	auto pressBefore = lvl.press;
	int savedStateCount = static_cast<int>(lvl.gameStates.size());
	auto savedPlayer = lvl.gameStates.back();

	constexpr int maxSimFrames = 1500;
	constexpr int stuckThreshold = 60;
	int simulatedFrame = frame;
	int endFrame = frame + maxSimFrames;
	float lastProgressX = lvl.latestState().pos.x;
	int framesSinceProgress = 0;

	while (!lvl.gameStates.back().dead && !isLevelEnd(lvl) && simulatedFrame < endFrame && !stop.load()) {
		if (inputs.contains(simulatedFrame))
			lvl.press = !lvl.press;
		lvl.runFrame(lvl.press);
		++simulatedFrame;

		float currentX = lvl.latestState().pos.x;
		if (currentX > lastProgressX + 0.5f) {
			lastProgressX = currentX;
			framesSinceProgress = 0;
		} else if (++framesSinceProgress >= stuckThreshold) {
			break;
		}
	}

	int finalFrame = lvl.currentFrame();
	float lastY = lvl.latestState().pos.y;
	bool completed = isLevelEnd(lvl);

	lvl.gameStates.resize(static_cast<size_t>(savedStateCount));
	lvl.gameStates.back() = std::move(savedPlayer);
	lvl.press = pressBefore;

	if (!completed && (lastY > 1300.0f || lastY < 0.0f))
		return 0;
	return finalFrame;
}

PathfindResult pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback) {
	Level2 lvl(lvlString);
	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<int> jitterDist(-10, 10);
	std::uniform_int_distribution<int> holdDist(2, 25);

	int trueBest = 0;
	int fail = 1;
	int numAway = 1000;
	std::vector<Player> bestStates = lvl.gameStates;
	int iterations = 100;
	constexpr int pairsPerTry = 8;

	auto toggleFrame = [](std::set<int>& inputs, int frame) {
		if (auto [it, inserted] = inputs.insert(frame); !inserted)
			inputs.erase(it); // Two transitions on one frame cancel each other.
	};

	while (!isLevelEnd(lvl) && !stop.load()) {
		auto frame = lvl.currentFrame();
		std::set<int> bestInputs;
		int bestFrame = frame;

		lvl.buildInterestingFrames();
		auto const& interesting = lvl.interestingFrames;
		auto firstInteresting = std::lower_bound(interesting.begin(), interesting.end(), frame);

		for (int iteration = 0; iteration < iterations && !stop.load(); ++iteration) {
			std::set<int> inputs;

			if (firstInteresting != interesting.end()) {
				std::uniform_int_distribution<size_t> indexDist(
					static_cast<size_t>(firstInteresting - interesting.begin()),
					interesting.size() - 1
				);
				for (int pair = 0; pair < pairsPerTry; ++pair) {
					int pressFrame = interesting[indexDist(rng)] + jitterDist(rng);
					if (pressFrame < frame)
						continue;
					toggleFrame(inputs, pressFrame);
					toggleFrame(inputs, pressFrame + holdDist(rng));
				}
			}

			int candidateFrame = tryInputs(lvl, inputs, stop);
			if (candidateFrame > bestFrame) {
				bestFrame = candidateFrame;
				bestInputs = std::move(inputs);
				if (bestFrame - frame > 500 && fail < 1000)
					break;
			}
		}

		if (stop.load())
			break;

		if (bestFrame == frame) {
			lvl.rollback(std::max(std::max(frame - fail, trueBest - numAway), 1));
			fail += 5;
			iterations = std::min(iterations + 30, 250);

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
			iterations = 100;
			int advanceTo = bestFrame - (bestFrame - frame) / 3;
			for (int i = frame; i < advanceTo && !isLevelEnd(lvl) && !stop.load(); ++i) {
				if (bestInputs.contains(i))
					lvl.press = !lvl.press;
				lvl.runFrame(lvl.press);
			}
		}

		if (lvl.currentFrame() > trueBest) {
			trueBest = lvl.currentFrame();
			fail = 0;
			numAway = 1000;
		}
		if (lvl.gameStates.size() > bestStates.size())
			bestStates = lvl.gameStates;
		if (callback) {
			double progress = lvl.length > 0.0f ? (lvl.latestState().pos.x / lvl.length) * 100.0 : 100.0;
			callback(std::clamp(progress, 0.0, 100.0));
		}
	}

	if (lvl.gameStates.size() > bestStates.size())
		bestStates = lvl.gameStates;

	Replay2 output;
	for (size_t index = 1; index < bestStates.size(); ++index) {
		auto const& state = bestStates[index];
		if (state.button != bestStates[index - 1].button)
			output.inputs.push_back(gdr::Input(state.frame, 1, false, state.button));
	}

	auto exported = output.exportData();
	if (exported.isErr())
		return {{}, "Failed to serialize macro data."};
	return {exported.unwrap(), {}};
}
