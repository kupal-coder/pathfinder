#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/**
 * The mod's own macro library.
 *
 * Solved macros are saved here automatically, so finishing a search no longer
 * drops you into a file picker aimed at some other mod's folder. Everything
 * lives in one directory under the mod's save dir and is listed by the in-game
 * browser.
 *
 * Exporting to a bot's folder is still available, but it is now a deliberate
 * action on a macro you already have rather than the only way to keep a result.
 */
namespace macrolib {

/// One macro on disk, plus the metadata needed to list it without re-reading.
struct MacroEntry {
	std::filesystem::path path;
	/// Level name as recorded in the replay, falling back to the file stem.
	std::string levelName;
	/// Level id, 0 when unknown (local levels).
	uint32_t levelId = 0;
	/// Percentage the route reached, 0-100.
	double percent = 0.0;
	/// Whether the route completes the level.
	bool solved = false;
	/// Number of input toggles.
	size_t inputCount = 0;
	/// Replay duration in seconds.
	float duration = 0.0f;
	/// Unix timestamp of last write, for sorting newest first.
	int64_t savedAt = 0;

	/// Filename without extension, used as the display/rename key.
	std::string stem() const { return path.stem().string(); }
};

/// Directory holding the library. Created on demand.
std::filesystem::path directory();

/// Load and sort the library, newest first.
std::vector<MacroEntry> list();

/**
 * Save a macro into the library.
 *
 * `preferredName` is sanitised and de-duplicated, so saving twice for the same
 * level produces "Level", "Level (2)", and so on rather than silently
 * overwriting an earlier attempt.
 *
 * Returns the path written, or an error string.
 */
geode::Result<std::filesystem::path> save(
	std::vector<uint8_t> const& data,
	std::string const& preferredName);

/// Delete a macro from the library.
geode::Result<> remove(std::filesystem::path const& path);

/// Rename a macro within the library, sanitising and de-duplicating the name.
geode::Result<std::filesystem::path> rename(
	std::filesystem::path const& path,
	std::string const& newName);

/// Copy a macro out to another location.
geode::Result<> exportTo(
	std::filesystem::path const& from,
	std::filesystem::path const& to);

/**
 * Where an installed bot keeps its replays, if one is detected.
 *
 * Used to offer a one-tap export to that bot. Returns nullopt when no known bot
 * is installed.
 */
std::optional<std::filesystem::path> detectedBotFolder();

/// Display name of the detected bot, for labelling the export button.
std::optional<std::string> detectedBotName();

/// Strip characters that are illegal in filenames on any supported platform.
std::string sanitiseName(std::string const& name);

} // namespace macrolib
