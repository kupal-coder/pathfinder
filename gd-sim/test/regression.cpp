#include <Level.hpp>
#include <Pad.hpp>
#include <Orb.hpp>
#include <Portals.hpp>
#include <cassert>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace {
std::unordered_map<int, std::string> objectFields(int id, float x = 100.0f, float y = 100.0f) {
    return {{1, std::to_string(id)}, {2, std::to_string(x)}, {3, std::to_string(y)}};
}
}

int main() {
    // Unsupported-only and empty levels must still be safe to simulate.
    Level empty("kA4,0;");
    empty.runFrame(false);
    assert(empty.currentFrame() == 2);

    // Level copies must own their history and point states back to the copy.
    Level source("kA4,0;1,1,2,500,3,-15;");
    source.runFrame(false);
    Level copy = source;
    assert(copy.latestState().level == &copy);
    copy.runFrame(true);
    assert(copy.latestState().level == &copy);

    // Teleport links in a copied level must target the copy, not the source level.
    Level teleportSource("kA4,0;1,747,2,100,3,15,57,7;1,748,2,200,3,15,57,7;");
    Level teleportCopy = teleportSource;
    std::vector<TeleportPortal*> copiedPortals;
    for (auto& section : teleportCopy.sections) {
        for (auto& object : section) {
            if (auto* portal = object->asTeleportPortal())
                copiedPortals.push_back(portal);
        }
    }
    assert(copiedPortals.size() == 2);
    assert(copiedPortals[0]->linkedPortal == copiedPortals[1]);
    assert(copiedPortals[1]->linkedPortal == copiedPortals[0]);

    // Unknown game modes must safely fall back rather than reaching UB in Vehicle::from.
    Level badVehicle("kA4,0,kA2,999;1,1,2,500,3,-15;");
    assert(badVehicle.latestState().vehicle.type == VehicleType::Cube);

    // Verify the formerly conflicting 2.1 object IDs are routed to concrete types.
    auto redPad = Object::create(objectFields(1332));
    assert(redPad && dynamic_cast<Pad*>(redPad->operator->()));
    auto spiderPortal = Object::create(objectFields(1331));
    auto* spider = spiderPortal ? dynamic_cast<VehiclePortal*>(spiderPortal->operator->()) : nullptr;
    assert(spider && spider->type == VehicleType::Spider);
    auto dash = Object::create(objectFields(1704));
    assert(dash && dynamic_cast<Orb*>(dash->operator->()));

    // Robot/Spider use cube impulse tables and must not throw velocity_map::at.
    Level robotPad("kA4,0,kA2,5;1,35,2,1,3,0;");
    robotPad.runFrame(false);

    std::cout << "gd-sim regression tests passed\n";
}
