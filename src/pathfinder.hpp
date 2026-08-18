#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct PathfindResult {
    std::vector<uint8_t> macro;
    std::string error;
};

PathfindResult pathfind(
    std::string const& lvlString,
    std::atomic_bool& stop,
    std::function<void(double)> callback
);
