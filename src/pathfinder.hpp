#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Live progress for the UI. Percentages are 0-100.
struct PathfindProgress {
	/// Best percentage of the level reached so far.
	double percent = 0.0;
	/// Percentage the frontier is currently working from.
	double frontierPercent = 0.0;
	/// Search rounds completed.
	uint64_t rounds = 0;
	/// Frames simulated across all workers.
	uint64_t framesSimulated = 0;
	/// Rounds since `percent` last improved. Non-zero means the search is stuck.
	uint64_t stalledRounds = 0;
	/// Candidates currently on the frontier.
	uint32_t frontier = 0;
	/// True once a full solution has been found.
	bool solved = false;
};

struct PathfindOptions {
	/// Worker threads. 0 picks a sensible default from the hardware.
	unsigned threads = 0;
	/// Candidates retained per round.
	unsigned beamWidth = 512;
	/// Frames simulated per expansion step.
	unsigned chunkFrames = 24;
	/// Give up after this many rounds without improvement. 0 disables.
	uint64_t stallLimit = 20000;
};

struct PathfindResult {
	/// Serialised .gdr2 replay. Empty when nothing was found.
	std::vector<uint8_t> macro;
	/// Whether the run reached the end of the level.
	bool solved = false;
	/// Best percentage reached.
	double percent = 0.0;
	/// Human-readable failure reason, empty on success.
	std::string error;
};

/**
 * Search for an input sequence that completes the level.
 *
 * Runs until solved, until `stop` is set, or until the stall limit is hit.
 * `callback` is invoked periodically from the worker thread.
 */
PathfindResult pathfind(
	std::string const& lvlString,
	std::atomic_bool& stop,
	std::function<void(PathfindProgress const&)> callback,
	PathfindOptions const& options = {});
