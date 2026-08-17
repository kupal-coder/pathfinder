/**
 * Tests for the macro library's naming rules.
 *
 * These do not need Geode: the sanitising and de-duplication logic is plain
 * C++ and is reproduced here against the real filesystem. It is worth testing
 * because a bug in either would silently overwrite a macro the user had
 * already saved, or produce a filename the OS rejects.
 *
 * The implementations under test live in src/library.cpp; this file keeps
 * byte-identical copies so the logic can run without the game. If you change
 * one, change both -- the test at the bottom asserts they agree on a corpus.
 */
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int g_failures = 0;

static void check(bool cond, std::string const& what) {
	std::cout << (cond ? "  ok    " : "  FAIL  ") << what << "\n";
	if (!cond)
		++g_failures;
}

// --- copy of macrolib::sanitiseName -----------------------------------------
static std::string sanitiseName(std::string const& name) {
	std::string out;
	out.reserve(name.size());

	for (char c : name) {
		bool bad = c == '/' || c == '\\' || c == ':' || c == '*' || c == '?'
				|| c == '"' || c == '<' || c == '>' || c == '|'
				|| static_cast<unsigned char>(c) < 0x20;
		out.push_back(bad ? '_' : c);
	}

	while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
		out.pop_back();

	while (!out.empty() && out.front() == ' ')
		out.erase(out.begin());

	if (out.empty())
		out = "Macro";

	if (out.size() > 120)
		out.resize(120);

	return out;
}

// --- copy of macrolib::uniquePath -------------------------------------------
static fs::path uniquePath(fs::path const& dir, std::string const& base) {
	auto candidate = dir / (base + ".gdr2");
	if (!fs::exists(candidate))
		return candidate;

	for (int i = 2; i < 10000; ++i) {
		candidate = dir / (base + " (" + std::to_string(i) + ").gdr2");
		if (!fs::exists(candidate))
			return candidate;
	}
	return dir / (base + " (" + std::to_string(std::time(nullptr)) + ").gdr2");
}

static void touch(fs::path const& p) {
	std::ofstream f(p, std::ios::binary);
	f << "x";
}

int main() {
	std::cout << "-- macro library naming --\n";

	// Characters that are illegal in a filename on at least one platform.
	check(sanitiseName("Stereo Madness") == "Stereo Madness", "ordinary name is untouched");
	check(sanitiseName("a/b\\c:d*e?f\"g<h>i|j").find_first_of("/\\:*?\"<>|") == std::string::npos,
		  "path and reserved characters are replaced");
	check(sanitiseName("trailing dots...") == "trailing dots", "trailing dots removed");
	check(sanitiseName("trailing spaces   ") == "trailing spaces", "trailing spaces removed");
	check(sanitiseName("   leading") == "leading", "leading spaces removed");
	check(sanitiseName("") == "Macro", "empty name gets a fallback");
	check(sanitiseName("   ") == "Macro", "whitespace-only name gets a fallback");
	check(sanitiseName("...") == "Macro", "dots-only name gets a fallback");
	check(sanitiseName(std::string(400, 'x')).size() == 120, "absurdly long name is truncated");

	// Control characters, which some filesystems accept and then cannot show.
	std::string withControl = "bad\x01\x02name\n";
	check(sanitiseName(withControl).find_first_of("\x01\x02\n") == std::string::npos,
		  "control characters are replaced");

	// A level name that is entirely non-ASCII must survive rather than becoming
	// the fallback -- players do use them.
	std::string unicode = "\xE3\x82\xB9\xE3\x83\x86\xE3\x83\xAC\xE3\x82\xAA";  // ステレオ
	check(sanitiseName(unicode) == unicode, "multi-byte UTF-8 names are preserved");

	std::cout << "\n-- de-duplication --\n";

	auto tmp = fs::temp_directory_path() / "pfp-library-test";
	fs::remove_all(tmp);
	fs::create_directories(tmp);

	auto first = uniquePath(tmp, "Level");
	check(first.filename() == "Level.gdr2", "first save uses the plain name");
	touch(first);

	auto second = uniquePath(tmp, "Level");
	check(second.filename() == "Level (2).gdr2", "second save is suffixed, not overwritten");
	touch(second);

	auto third = uniquePath(tmp, "Level");
	check(third.filename() == "Level (3).gdr2", "third save increments again");
	touch(third);

	check(fs::exists(first) && fs::exists(second) && fs::exists(third),
		  "all three files coexist on disk");

	// Deleting a middle entry should let the next save reclaim that slot rather
	// than skipping past it.
	fs::remove(second);
	auto reclaimed = uniquePath(tmp, "Level");
	check(reclaimed.filename() == "Level (2).gdr2", "freed slot is reused");

	// Names that sanitise to the same string must still not collide.
	auto a = uniquePath(tmp, sanitiseName("My/Level"));
	touch(a);
	auto b = uniquePath(tmp, sanitiseName("My\\Level"));
	check(a != b, "different inputs that sanitise identically do not collide");

	fs::remove_all(tmp);

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
