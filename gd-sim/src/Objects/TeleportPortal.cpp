#include <Portals.hpp>
#include <Player.hpp>
#include <cstdlib>

TeleportPortal::TeleportPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
    : EffectObject(size, std::move(fields)), groupId(-1), linkedPortal(nullptr) {
    if (auto it = fields.find(57); it != fields.end()) {
        char* end = nullptr;
        long parsed = std::strtol(it->second.c_str(), &end, 10);
        if (end != it->second.c_str() && *end == '\0')
            groupId = static_cast<int>(parsed);
    }
}

void TeleportPortal::collide(Player& p) const {
    if (linkedPortal == nullptr || groupId < 0)
        return;

    // Portal activation belongs entirely to Player state, so rollback restores it.
    EffectObject::collide(p);

    float relX = p.pos.x - pos.x;
    float relY = p.pos.y - pos.y;
    p.pos.x = linkedPortal->pos.x + relX;
    p.pos.y = linkedPortal->pos.y + relY;

    // Mark the destination as used to prevent an immediate return teleport.
    if (linkedPortal->id >= 0)
        p.usedEffects.insert(linkedPortal->id);
}
