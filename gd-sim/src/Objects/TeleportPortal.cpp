#include <Portals.hpp>
#include <Player.hpp>

TeleportPortal::TeleportPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
    : EffectObject(size, std::move(fields)), linkedPortal(nullptr), cooldown(0) {
    // Group ID is stored in field 57 for teleport portals
    auto it = fields.find(57);
    if (it != fields.end()) {
        groupId = std::stoi(it->second);
    } else {
        groupId = -1;  // no group = can't link
    }
}

void TeleportPortal::collide(Player& p) const {
    // Handle cooldown decrement (mutable so we can update in const method)
    if (cooldown > 0) {
        cooldown--;
        return;
    }

    // Must have a linked portal to teleport to
    if (linkedPortal == nullptr || groupId < 0) {
        return;
    }

    // Don't teleport if already used this frame via EffectObject
    EffectObject::collide(p);

    // Calculate relative position within this portal
    float relX = p.pos.x - pos.x;
    float relY = p.pos.y - pos.y;

    // Also account for velocity direction to place player correctly on exit
    // Preserve the player's velocity
    float savedVelocity = p.velocity;

    // Teleport: move player to linked portal's position, maintaining relative offset
    p.pos.x = linkedPortal->pos.x + relX;
    p.pos.y = linkedPortal->pos.y + relY;

    // Restore velocity (teleport doesn't change it)
    p.velocity = savedVelocity;

    // Set cooldown on both portals to prevent immediate re-teleport
    cooldown = 10;
    linkedPortal->cooldown = 10;

    // Also mark both portals as used via usedEffects
    if (linkedPortal->id >= 0) {
        p.usedEffects.insert(linkedPortal->id);
    }
}
