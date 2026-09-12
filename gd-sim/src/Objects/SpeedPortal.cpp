#include <Portals.hpp>
#include <Player.hpp>
#include <algorithm>

SpeedPortal::SpeedPortal(Vec2D size, std::unordered_map<int, std::string>&& fields) : EffectObject(size, std::move(fields)) {
	switch (atoi(fields[1].c_str())) {
		case 200:
			speed = 0;
			break;
		case 201:
			speed = 1;
			break;
		case 202:
			speed = 2;
			break;
		case 203:
			speed = 3;
			break;
		case 1334:
			speed = 4;
			break;
	}
}


void SpeedPortal::collide(Player& p) const {
	EffectObject::collide(p);

	p.speed = std::clamp(speed, 0, 4);
}
