#include <atomic>
#include <string>
#include <functional>
#include <memory>

struct RuntimeLevelSnapshot;

std::vector<uint8_t> pathfind(
    std::string const& lvlString,
    std::atomic_bool& stop,
    std::function<void(double)> callback,
    std::shared_ptr<RuntimeLevelSnapshot const> runtimeSnapshot = {}
);