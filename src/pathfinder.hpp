#include <atomic>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

struct PathfindResult {
    std::vector<uint8_t> replay;
    double progress = 0.0;
    bool solved = false;
    // Tightest frame gap between any exported input and the death that ended
    // the run. 0 when the level was solved. Used to warn about fragile macros.
    float minMargin = 0.0f;
    std::string error;
    // Non-blocking heads-up: the level contains things the sim can't model
    // (e.g. Move/Rotate triggers) so the exported macro may not match the
    // real level even though it solves the simulated one.
    std::string warning;
    // Solver diagnostics (C7): wall-clock time, simulated frames, branch
    // count and the random seed used. Re-running with the same seed on the
    // same level reproduces the same search.
    long wallTimeMs = 0;
    long framesSimulated = 0;
    int branchesUsed = 0;
    uint32_t seedUsed = 0;
    // Number of inputs in the exported macro. 0 with a non-empty replay
    // means a valid but input-less file; 0 bytes means nothing was encoded.
    size_t inputsRecorded = 0;
    // Sim coverage: level objects with a sim mapping vs without. Unmapped
    // objects are decorations/triggers — or blocks the sim doesn't know yet
    // (desync suspect #1; include topIgnoredIds in bug reports).
    size_t objectsModelled = 0;
    size_t objectsIgnored = 0;
    // Most common unmapped object ids, "id x count" formatted.
    std::string topIgnoredIds;
};

PathfindResult pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback, int inputOffset = 0, int solverSeed = 0);
