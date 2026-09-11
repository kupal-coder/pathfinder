#include <atomic>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

struct PathfindResult {
    std::vector<uint8_t> replay;
    double progress = 0.0;
    bool solved = false;
    std::string error;
};

PathfindResult pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback);
