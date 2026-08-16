#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Tunables the mod passes down from its settings.
struct PathfindOptions {
	/// Hard ceiling on clicks per second. 240 physics frames per second, so a
	/// cap of 15 means presses are at least 16 frames apart.
	int maxCps = 15;

	/// Shortest press, in frames. Longer taps replay more reliably, but forcing
	/// a minimum costs solve rate on frame-perfect sections, so this defaults to
	/// no minimum and the hardening pass lengthens taps where it can.
	int minHoldFrames = 1;

	/// Spend extra time making the timings survive a frame of slop, which is
	/// what CBF's sub-frame input placement introduces.
	bool harden = true;

	/// 0 = decide from the hardware.
	unsigned threads = 0;
};

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

	/// Highest clicks-per-second over any one second window of the macro.
	float peakCps = 0;

	/// Share of jittered replays that still finished, 0-100. 100 means every
	/// input can land a frame early or late and the macro still works, which is
	/// what makes it survive Click Between Frames.
	float robustness = 0;

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
 * @param options    click rate limits and hardening
 */
PathfindResult pathfind(
	std::string const& lvlString,
	std::atomic_bool& stop,
	std::function<void(double)> callback,
	std::string const& levelName = "",
	PathfindOptions const& options = {}
);
