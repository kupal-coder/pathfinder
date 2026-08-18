#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

std::vector<uint8_t> pathfind(
    std::string const& lvlString,
    std::atomic_bool& stop,
    std::function<void(double)> callback
);
