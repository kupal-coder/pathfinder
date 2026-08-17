#include <atomic>
#include <string>
#include <functional>

std::vector<uint8_t> pathfind(std::string const& lvlString, std::atomic_bool& stop, std::function<void(double)> callback);