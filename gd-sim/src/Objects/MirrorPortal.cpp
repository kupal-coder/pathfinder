#include <Portals.hpp>
#include <Player.hpp>

MirrorPortal::MirrorPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
    : EffectObject(size, std::move(fields)), flipped(std::stoi(fields[1]) == 45) {}

void MirrorPortal::collide(Player& p) const {
    EffectObject::collide(p);
    p.mirror = flipped;
}
