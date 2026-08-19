#include <Hazard.hpp>
#include <Player.hpp>
#include <cmath>
#include <algorithm>

void Hazard::collide(Player& player) const {
    player.dead = true;
}

bool Sawblade::touching(Player const& player) const {
    // 1. Get player center and half-extents
    Entity playerHitbox = player.innerHitbox();
    float pCenterX = playerHitbox.x + playerHitbox.width * 0.5f;
    float pCenterY = playerHitbox.y + playerHitbox.height * 0.5f;
    float pHalfW   = playerHitbox.width * 0.5f;
    float pHalfH   = playerHitbox.height * 0.5f;

    // 2. Sawblade center and fatal radius (GD saw fatal radius is ~60% of size)
    float sCenterX = this->x + this->width * 0.5f;
    float sCenterY = this->y + this->height * 0.5f;
    float sRadius  = (this->width * 0.5f) * 0.60f; 

    // 3. Find closest point on Player AABB to Sawblade Center
    float closestX = std::clamp(sCenterX, pCenterX - pHalfW, pCenterX + pHalfW);
    float closestY = std::clamp(sCenterY, pCenterY - pHalfH, pCenterY + pHalfH);

    // 4. Circle-to-box distance check
    float dx = sCenterX - closestX;
    float dy = sCenterY - closestY;
    return (dx * dx + dy * dy) <= (sRadius * sRadius);
}