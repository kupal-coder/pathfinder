#pragma once
#include <string>
#include <vector>

/**
 * Golden-file regression scenarios.
 *
 * Each scenario is a procedurally built level string plus an input string
 * ('0'/'1' per frame). Traces are recorded from a known-good build and then
 * compared byte-for-byte on every later build. This is what makes it safe to
 * refactor the physics plumbing: if a trace moves by a single ULP, the test
 * fails and names the frame.
 *
 * Scenarios deliberately span the feature surface the simulator claims to
 * support -- blocks, spikes, slopes, orbs, pads, and every portal type -- and
 * a few pathological parse cases.
 */
struct Scenario {
    std::string name;
    std::string level;
    std::string inputs;
};

namespace scenariodetail {

inline std::string obj(int id, double x, double y) {
    return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y) + ";";
}

inline std::string ground(int len, double y = -15) {
    std::string s;
    for (int x = 0; x < len; x += 30)
        s += obj(1, x, y);
    return s;
}

/// 'p' = press pattern helper: hold for `on` frames, release for `off`.
inline std::string cycle(int frames, int on, int off) {
    std::string s;
    for (int i = 0; i < frames; ++i)
        s += ((i % (on + off)) < on) ? '1' : '0';
    return s;
}

inline std::string hold(int frames) { return std::string(frames, '1'); }
inline std::string release(int frames) { return std::string(frames, '0'); }

} // namespace scenariodetail

inline std::vector<Scenario> allScenarios() {
    using namespace scenariodetail;
    std::vector<Scenario> out;

    // --- Cube: flat ground, no input. Baseline gravity/ground contact. ---
    out.push_back({"cube_idle",
        "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(4000),
        release(1200)});

    // --- Cube: repeated jumps. Exercises jump velocity, coyote frames. ---
    out.push_back({"cube_jump_cycle",
        "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000),
        cycle(2400, 3, 25)});

    // --- Cube over spikes: hazard collision + death path. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000);
        for (int x = 600; x < 7000; x += 300)
            lvl += obj(8, x, 15);
        out.push_back({"cube_spikes", lvl, cycle(2400, 4, 40)});
    }

    // --- Blocks to land on: block collision, x-snapping, stair snapping. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000);
        for (int i = 0; i < 20; ++i) {
            double bx = 500 + i * 300;
            lvl += obj(1, bx, 45);
            lvl += obj(1, bx + 30, 75);
            lvl += obj(1, bx + 60, 105);
        }
        out.push_back({"cube_stairs", lvl, cycle(2400, 5, 30)});
    }

    // --- Slopes (both 30x30 and 60x30 variants). ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000);
        for (int i = 0; i < 12; ++i) {
            double sx = 600 + i * 500;
            lvl += obj(289, sx, 45);
            lvl += obj(291, sx + 60, 45);
        }
        out.push_back({"cube_slopes", lvl, cycle(2400, 6, 50)});
    }

    // --- Ship: portal entry then held/released flight. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += obj(13, 300, 105);
        out.push_back({"ship_flight", lvl, cycle(2400, 12, 12)});
    }

    // --- Ball: portal entry, gravity flipping on click. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += obj(47, 300, 105);
        out.push_back({"ball_roll", lvl, cycle(2400, 3, 45)});
    }

    // --- UFO: portal entry, repeated flaps. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += obj(111, 300, 105);
        out.push_back({"ufo_flap", lvl, cycle(2400, 2, 20)});
    }

    // --- Wave: portal entry, zigzag. Wave has its own size + clamp logic. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += obj(660, 300, 105);
        out.push_back({"wave_zigzag", lvl, cycle(2400, 8, 8)});
    }

    // --- Orbs: yellow, blue, pink, red, green, black. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        int ids[] = {36, 84, 141, 1333, 1022, 1330};
        for (int i = 0; i < 6; ++i)
            lvl += obj(ids[i], 600 + i * 400, 60);
        out.push_back({"orbs_all", lvl, cycle(2400, 4, 20)});
    }

    // --- Pads: yellow, blue, pink. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        int ids[] = {35, 67, 140};
        for (int i = 0; i < 3; ++i)
            lvl += obj(ids[i], 700 + i * 700, 15);
        out.push_back({"pads_all", lvl, release(2400)});
    }

    // --- Gravity portals: flip and flip back. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += ground(9000, 315);  // ceiling to land on when flipped
        lvl += obj(11, 600, 105);
        lvl += obj(10, 2400, 105);
        out.push_back({"gravity_portals", lvl, cycle(2400, 3, 40)});
    }

    // --- Speed portals: all five speeds in sequence. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(12000);
        int ids[] = {200, 201, 202, 203, 1334};
        for (int i = 0; i < 5; ++i)
            lvl += obj(ids[i], 500 + i * 900, 45);
        out.push_back({"speed_portals", lvl, cycle(2400, 4, 30)});
    }

    // --- Size portal: mini and back. Hitbox resize mid-run. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += obj(101, 600, 45);
        lvl += obj(99, 3000, 45);
        out.push_back({"size_portals", lvl, cycle(2400, 4, 30)});
    }

    // --- Level settings: start as mini upside-down ship at 2x. ---
    out.push_back({"settings_mini_ship",
        "kA4,2,kA3,1,kA11,1,kA2,1;" + ground(9000) + ground(9000, 315),
        cycle(2000, 10, 10)});

    // --- Breakable blocks. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000);
        for (int i = 0; i < 10; ++i)
            lvl += obj(143, 600 + i * 400, 15);
        out.push_back({"breakable_blocks", lvl, cycle(2000, 4, 30)});
    }

    // --- Sawblades (hazard with radial hitbox). ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(8000);
        for (int i = 0; i < 10; ++i)
            lvl += obj(88, 700 + i * 500, 15);
        out.push_back({"sawblades", lvl, cycle(2000, 5, 35)});
    }

    // --- Start position object (id 31) overriding spawn. ---
    {
        std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0;" + ground(9000);
        lvl += "1,31,2,1500,3,15,kA4,1,kA3,0,kA11,0,kA2,0;";
        out.push_back({"start_pos", lvl, cycle(2000, 4, 30)});
    }

    return out;
}

/// Level strings that must not crash the parser. Not trace-compared.
inline std::vector<std::pair<std::string, std::string>> malformedLevels() {
    return {
        {"truncated_object",   "kA4,0;1,1,2,0,3,15;1"},
        {"empty_object",       "kA4,0;1,1,2,0,3,15;,,,;"},
        {"non_numeric_fields", "kA4,0;1,1,2,0,3,15;2,x,3,y;"},
        {"missing_id",         "kA4,0;2,100,3,15;"},
        {"empty_string",       ""},
        {"only_settings",      "kA4,0,kA3,0,kA11,0,kA2,0;"},
        {"garbage",            "!!!!;;;;????;"},
        {"huge_id",            "kA4,0;1,999999999999,2,0,3,15;"},
        {"negative_coords",    "kA4,0;1,1,2,-500,3,-500;"},
        {"trailing_commas",    "kA4,0;1,1,2,0,3,15,,,,;"},
    };
}
