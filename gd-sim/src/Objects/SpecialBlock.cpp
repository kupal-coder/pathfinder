#include <Block.hpp>
#include <Player.hpp>

SpecialBlock::SpecialBlock(Vec2D size, std::unordered_map<int, std::string>&& fields) : Object(size, std::move(fields)) {
	prio = 0;
	int objId = stoi_def(fields, 1);
	switch (objId) {
		case 1813: type = SpecialBlockType::S; break;
		case 1814: type = SpecialBlockType::J; break;
		case 1815: type = SpecialBlockType::H; break;
		case 1816: type = SpecialBlockType::D; break;
		default:   type = SpecialBlockType::S; break;
	}
}

void SpecialBlock::collide(Player& p) const {
	switch (type) {
		case SpecialBlockType::S:
			// S-Block stops Dash immediately
			p.isDashing = false;
			break;
		case SpecialBlockType::J:
			// J-Block prevents jump buffering
			p.hasJBlock = true;
			p.buffer = false;
			p.input = false;
			break;
		case SpecialBlockType::H:
			// H-Block prevents Cube/Robot head collision death
			p.hasHBlock = true;
			break;
		case SpecialBlockType::D:
			// D-Block enables Wave to slide on blocks
			p.hasDBlock = true;
			break;
	}
}
