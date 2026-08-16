#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Everything the UI needs to know about a pathfinding attempt.
struct PathfindResult {
	/// Encoded .gdr2 macro. Empty when nothing usable was found.
	std::vector<uint8_t> macro;

	/// True only when the run reached the end of the level *and* the exported
	/// inputs were replayed from frame 0 and confirmed to still reach it.
	bool completed = false;

	/// True when the exported inputs were replayed and matched the search result.
	bool verified = false;

	/// How far the best attempt got, 0-100.
	float progress = 0;

	/// Length of the best attempt in frames, and how many clicks it needs.
	uint32_t frames = 0;
	uint32_t clicks = 0;

	/// Human readable notes about things the simulator could not model.
	std::vector<std::string> warnings;
};

/**
 * Searches for an input sequence that completes the level.
 *
 * @param lvlString  decompressed level string
 * @param stop       set to true to abort; the best attempt so far is returned
 * @param callback   progress in percent, called from the worker thread
 * @param levelName  stored in the macro metadata
 */
PathfindResult pathfind(
	std::string const& lvlString,
	std::atomic_bool& stop,
	std::function<void(double)> callback,
	std::string const& levelName = ""
);
