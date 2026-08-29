#include <atomic>
#include <string>
#include <functional>
#include <vector>
#include <cstdint>

std::vector<uint8_t> pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback);