#include "library.hpp"
#include "replay.hpp"

#include <algorithm>
#include <system_error>

using namespace geode::prelude;

namespace macrolib {

std::filesystem::path directory() {
	auto dir = Mod::get()->getSaveDir() / "macros";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir;
}

std::string sanitiseName(std::string const& name) {
	std::string out;
	out.reserve(name.size());

	for (char c : name) {
		// Reject path separators, Windows-reserved characters and control
		// codes. A level name is arbitrary user text and can contain anything.
		bool bad = c == '/' || c == '\\' || c == ':' || c == '*' || c == '?'
				|| c == '"' || c == '<' || c == '>' || c == '|'
				|| static_cast<unsigned char>(c) < 0x20;
		out.push_back(bad ? '_' : c);
	}

	// Trailing dots and spaces are not addressable on Windows.
	while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
		out.pop_back();

	while (!out.empty() && out.front() == ' ')
		out.erase(out.begin());

	if (out.empty())
		out = "Macro";

	// Keep well under the common 255-byte filename limit, leaving room for the
	// " (12)" suffix and the extension.
	if (out.size() > 120)
		out.resize(120);

	return out;
}

/// First free "name", "name (2)", "name (3)"... in the library.
static std::filesystem::path uniquePath(std::string const& base) {
	auto dir = directory();
	auto candidate = dir / (base + ".gdr2");
	if (!std::filesystem::exists(candidate))
		return candidate;

	for (int i = 2; i < 10000; ++i) {
		candidate = dir / fmt::format("{} ({}).gdr2", base, i);
		if (!std::filesystem::exists(candidate))
			return candidate;
	}
	// Absurdly unlikely; fall back to something unique enough to not clobber.
	return dir / fmt::format("{} ({}).gdr2", base, std::time(nullptr));
}

Result<std::filesystem::path> save(
	std::vector<uint8_t> const& data,
	std::string const& preferredName) {

	if (data.empty())
		return Err("Macro is empty");

	auto path = uniquePath(sanitiseName(preferredName));

	auto res = file::writeBinary(path, data);
	if (!res)
		return Err(res.unwrapErr());

	return Ok(path);
}

Result<> remove(std::filesystem::path const& path) {
	std::error_code ec;
	if (!std::filesystem::remove(path, ec))
		return Err(ec ? ec.message() : std::string("File not found"));
	return Ok();
}

Result<std::filesystem::path> rename(
	std::filesystem::path const& path,
	std::string const& newName) {

	auto clean = sanitiseName(newName);
	if (clean == path.stem().string())
		return Ok(path);

	auto target = uniquePath(clean);

	std::error_code ec;
	std::filesystem::rename(path, target, ec);
	if (ec)
		return Err(ec.message());

	return Ok(target);
}

Result<> exportTo(std::filesystem::path const& from, std::filesystem::path const& to) {
	std::error_code ec;

	if (auto parent = to.parent_path(); !parent.empty())
		std::filesystem::create_directories(parent, ec);

	ec.clear();
	std::filesystem::copy_file(
		from, to, std::filesystem::copy_options::overwrite_existing, ec);

	if (ec)
		return Err(ec.message());

	return Ok();
}

/// Bots we know how to hand a replay to, in priority order.
struct BotTarget {
	char const* modId;
	char const* displayName;
	char const* subfolder;
};

static constexpr BotTarget kBots[] = {
	{"eclipse.eclipse-menu", "Eclipse", "replays"},
};

std::optional<std::filesystem::path> detectedBotFolder() {
	for (auto const& bot : kBots) {
		if (auto mod = Loader::get()->getLoadedMod(bot.modId)) {
			auto dir = mod->getSaveDir();
			if (bot.subfolder && *bot.subfolder)
				dir /= bot.subfolder;
			return dir;
		}
	}
	return std::nullopt;
}

std::optional<std::string> detectedBotName() {
	for (auto const& bot : kBots) {
		if (Loader::get()->isModLoaded(bot.modId))
			return std::string(bot.displayName);
	}
	return std::nullopt;
}

std::vector<MacroEntry> list() {
	std::vector<MacroEntry> out;

	auto dir = directory();
	std::error_code ec;
	if (!std::filesystem::exists(dir, ec))
		return out;

	for (auto const& item : std::filesystem::directory_iterator(dir, ec)) {
		if (ec)
			break;
		if (!item.is_regular_file())
			continue;
		if (item.path().extension() != ".gdr2")
			continue;

		MacroEntry entry;
		entry.path = item.path();
		entry.levelName = item.path().stem().string();

		std::error_code timeEc;
		auto written = std::filesystem::last_write_time(item.path(), timeEc);
		if (!timeEc) {
			// Portable-ish conversion; only used for ordering and display.
			auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
				written - std::filesystem::file_time_type::clock::now()
					+ std::chrono::system_clock::now());
			entry.savedAt = std::chrono::duration_cast<std::chrono::seconds>(
				sys.time_since_epoch()).count();
		}

		// Metadata comes from the replay itself where possible. A file that
		// fails to parse is still listed -- it is better to show it and let the
		// user delete it than to silently hide a file that exists on disk.
		if (auto data = file::readBinary(item.path())) {
			auto parsed = PathfinderReplay::importData(data.unwrap());
			if (parsed.isOk()) {
				auto replay = parsed.unwrap();
				if (!replay.levelInfo.name.empty())
					entry.levelName = replay.levelInfo.name;
				entry.levelId = replay.levelInfo.id;
				entry.duration = replay.duration;
				entry.inputCount = replay.inputs.size();

				// The solver records progress in the description as a plain
				// "NN.NN%" so it survives a round trip through any bot.
				auto const& desc = replay.description;
				if (auto pos = desc.find('%'); pos != std::string::npos) {
					try {
						entry.percent = std::stod(desc.substr(0, pos));
					} catch (...) {
						entry.percent = 0.0;
					}
				}
				entry.solved = entry.percent >= 99.995;
			}
		}

		out.push_back(std::move(entry));
	}

	// Newest first: the macro you just made should be at the top.
	std::sort(out.begin(), out.end(), [](MacroEntry const& a, MacroEntry const& b) {
		if (a.savedAt != b.savedAt)
			return a.savedAt > b.savedAt;
		return a.levelName < b.levelName;
	});

	return out;
}

} // namespace macrolib
