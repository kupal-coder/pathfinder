#include <Portals.hpp>
#include <Player.hpp>

DualPortal::DualPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
    : EffectObject(size, std::move(fields)), dual(std::stoi(fields[1]) == 286) {}

void DualPortal::collide(Player& p) const {
    EffectObject::collide(p);
    p.dual = dual;
}
