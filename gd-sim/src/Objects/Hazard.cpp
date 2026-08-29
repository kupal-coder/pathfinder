#include <Hazard.hpp>
#include <Player.hpp>
#include <cmath>
#include <algorithm>

Hazard::Hazard(Vec2D size, std::unordered_map<int, std::string>&& fields) : Object(size, std::move(fields)) {
    prio = 2;
}

void Hazard::collide(Player& player) const {
    player.dead = true;
}

bool Sawblade::touching(Player const& player) const {
    // 1. Get player center and half-extents (Entity::pos is the center)
    Entity playerHitbox = player.innerHitbox();
    float pCenterX = playerHitbox.pos.x;
    float pCenterY = playerHitbox.pos.y;
    float pHalfW   = playerHitbox.size.x * 0.5f;
    float pHalfH   = playerHitbox.size.y * 0.5f;

    // 2. Sawblade center and fatal radius (GD saw fatal radius is ~60% of size)
    float sCenterX = this->pos.x;
    float sCenterY = this->pos.y;
    float sRadius  = (this->size.x * 0.5f) * 0.60f; 

    // 3. Find closest point on Player AABB to Sawblade Center
    float closestX = std::clamp(sCenterX, pCenterX - pHalfW, pCenterX + pHalfW);
    float closestY = std::clamp(sCenterY, pCenterY - pHalfH, pCenterY + pHalfH);

    // 4. Circle-to-box distance check
    float dx = sCenterX - closestX;
    float dy = sCenterY - closestY;
    return (dx * dx + dy * dy) <= (sRadius * sRadius);
}

bool Spike::touching(Player const& player) const {
    return Object::touching(player);
}