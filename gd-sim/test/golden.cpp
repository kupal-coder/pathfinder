/**
 * Golden-file regression runner for gd-sim.
 *
 *   golden record   -- write traces to gd-sim/test/golden/
 *   golden verify   -- re-run and diff against recorded traces (exit 1 on drift)
 *
 * `verify` is what CI runs. It also exercises the malformed-level corpus to
 * ensure the parser never throws.
 */
#include "scenarios.hpp"
#include "trace.hpp"

#include <Level.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static fs::path goldenDir() {
    return fs::path(__FILE__).parent_path() / "golden";
}

static std::string readFile(fs::path const& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

/// Report the first differing line so failures point at a frame, not a file.
static void reportDiff(std::string const& name, std::string const& expected, std::string const& actual) {
    std::istringstream e(expected), a(actual);
    std::string le, la;
    size_t line = 0;
    while (true) {
        bool he = static_cast<bool>(std::getline(e, le));
        bool ha = static_cast<bool>(std::getline(a, la));
        ++line;
        if (!he && !ha) break;
        if (le != la) {
            std::cerr << "    first difference at line " << line << ":\n"
                      << "      expected: " << (he ? le : "<eof>") << "\n"
                      << "      actual:   " << (ha ? la : "<eof>") << "\n";
            return;
        }
        if (!he || !ha) break;
    }
}

static int runMalformed() {
    int failures = 0;
    std::cout << "-- malformed level strings (parser must not throw) --\n";
    for (auto const& [name, lvlString] : malformedLevels()) {
        try {
            Level lvl(lvlString);
            // Also step it: a level that parses but explodes on frame 1 is no good.
            for (int i = 0; i < 20; ++i)
                lvl.runFrame(i % 3 == 0);
            std::cout << "  ok    " << name << " (objects " << lvl.objectCount << ")\n";
        } catch (std::exception const& ex) {
            std::cout << "  FAIL  " << name << " threw: " << ex.what() << "\n";
            ++failures;
        } catch (...) {
            std::cout << "  FAIL  " << name << " threw unknown exception\n";
            ++failures;
        }
    }
    return failures;
}

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "verify";
    auto dir = goldenDir();

    if (mode == "record") {
        fs::create_directories(dir);
        for (auto const& sc : allScenarios()) {
            auto text = traceRun(sc.level, sc.inputs);
            std::ofstream out(dir / (sc.name + ".txt"), std::ios::binary);
            out << text;
            std::cout << "recorded " << sc.name << " (" << text.size() << " bytes)\n";
        }
        std::cout << "\nGolden traces written to " << dir << "\n";
        return 0;
    }

    if (mode != "verify") {
        std::cerr << "usage: golden [record|verify]\n";
        return 2;
    }

    int failures = 0;
    int checked = 0;

    std::cout << "-- physics golden traces --\n";
    for (auto const& sc : allScenarios()) {
        auto path = dir / (sc.name + ".txt");
        if (!fs::exists(path)) {
            std::cout << "  MISS  " << sc.name << " (no golden file; run `golden record`)\n";
            ++failures;
            continue;
        }

        std::string expected = readFile(path);
        std::string actual;
        try {
            actual = traceRun(sc.level, sc.inputs);
        } catch (std::exception const& ex) {
            std::cout << "  FAIL  " << sc.name << " threw: " << ex.what() << "\n";
            ++failures;
            continue;
        }

        ++checked;
        if (actual == expected) {
            std::cout << "  ok    " << sc.name << "\n";
        } else {
            std::cout << "  FAIL  " << sc.name << " trace drifted\n";
            reportDiff(sc.name, expected, actual);
            ++failures;
        }
    }

    failures += runMalformed();

    std::cout << "\n" << checked << " traces checked, " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
