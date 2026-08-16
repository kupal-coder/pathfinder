#include <sstream>
#include <iomanip>
#include <limits>
#include <Level.hpp>

void Level::initLevelSettings(std::string const& lvlSettings, Player& player) {
	std::unordered_map<std::string, std::string> obj;

	std::stringstream ss2(lvlSettings);
	std::string k, v;
	while (std::getline(ss2, k, ',')) {
		std::getline(ss2, v, ',');
		obj[k] = v;
	}

	// Helper to make a default value for nonexistant keys
	auto get_or = [&obj](std::string const& key, std::string const& def) {
		if (auto it = obj.find(key); it != obj.end())
			return it->second.c_str();
		return def.c_str();
	};

	player.speed = atoi(get_or("kA4", "0"));

	// Robtop stores 1x speed as 0 and slow speed as 1. Very silly
	if (player.speed == 0)
		player.speed = 1;
	else if (player.speed == 1)
		player.speed = 0;

	if ((player.small = atoi(get_or("kA3", "0"))))
		player.size = player.size * 0.6;

	player.upsideDown = atoi(get_or("kA11", "0"));
	player.vehicle = Vehicle::from(static_cast<VehicleType>(atoi(get_or("kA2", "0"))));

	player.floor = 0;
	player.ceiling = player.vehicle.bounds;
}

Level::Level(std::string const& lvlString) {
	// Split by ';' to parse level string
	std::stringstream ss(lvlString);
	std::string objstr;
	bool first = true;

	// First player state
	auto player = Player();

	while (std::getline(ss, objstr, ';')) {
		// First entry is level settings object
		if (first) {
			initLevelSettings(objstr, player);
			first = false;
			continue;
		}

		std::unordered_map<int, std::string> obj;

		std::stringstream ss2(objstr);
		std::string k, v;
		while (std::getline(ss2, k, ',')) {
			std::getline(ss2, v, ',');
			if (atoi(k.c_str()) > 0)
				obj[atoi(k.c_str())] = v;
		}

		if (obj[1] == "31") {
			initLevelSettings(objstr, player);
			player.pos.x = stod_def(obj[2], 0);
			player.pos.y = stod_def(obj[3], 0);
		}

		// Read the id before the map is moved into create()
		int objectId = atoi(obj[1].c_str());

		if (auto ob_o = Object::create(std::move(obj))) {
			auto ob = ob_o.value();

			// Unique ID
			ob->id = objectCount++;

			// Sections are divided by x position in increments of 100
			size_t sectionPos = std::max(.0f, ob->pos.x / sectionSize);
			if (sectionPos >= sections.size())
				sections.resize(sectionPos + 1);
			sections[sectionPos].push_back(ob);

			if (ob->pos.x > length)
				length = ob->pos.x + 100;
		} else if (objectId > 0) {
			// Nothing in the simulator matches this id, so it will be invisible to
			// the physics. Pathfinder reports these so a desync can be explained.
			unknownObjects[objectId]++;
		}
	}

	player.level = this;
	gameStates.push_back(player);
}

void Level::rebind() {
	for (auto& p : gameStates)
		p.level = this;
	for (auto& p : gameStates2)
		p.level = this;
}

Level::Level(Level const& other) :
	gameStates(other.gameStates), gameStates2(other.gameStates2),
	dualStartFrame(other.dualStartFrame), dual(other.dual),
	objectCount(other.objectCount), sections(other.sections),
	unknownObjects(other.unknownObjects), length(other.length), debug(other.debug) {
	rebind();
}

Level& Level::operator=(Level const& other) {
	if (this == &other)
		return *this;

	gameStates = other.gameStates;
	gameStates2 = other.gameStates2;
	dualStartFrame = other.dualStartFrame;
	dual = other.dual;
	objectCount = other.objectCount;
	sections = other.sections;
	unknownObjects = other.unknownObjects;
	length = other.length;
	debug = other.debug;
	rebind();
	return *this;
}

Level::Level(Level&& other) noexcept :
	gameStates(std::move(other.gameStates)), gameStates2(std::move(other.gameStates2)),
	dualStartFrame(other.dualStartFrame), dual(other.dual),
	objectCount(other.objectCount), sections(std::move(other.sections)),
	unknownObjects(std::move(other.unknownObjects)), length(other.length), debug(other.debug) {
	rebind();
}

Level& Level::operator=(Level&& other) noexcept {
	if (this == &other)
		return *this;

	gameStates = std::move(other.gameStates);
	gameStates2 = std::move(other.gameStates2);
	dualStartFrame = other.dualStartFrame;
	dual = other.dual;
	objectCount = other.objectCount;
	sections = std::move(other.sections);
	unknownObjects = std::move(other.unknownObjects);
	length = other.length;
	debug = other.debug;
	rebind();
	return *this;
}

void Level::stepPlayer(Player& p, bool pressed, float dt) {
	p.dt = dt;
	p.preCollision(pressed);

	// A level with no simulated objects at all has no sections to index into.
	if (sections.empty()) {
		if (!p.dead)
			p.postCollision();
		return;
	}

	// Objects from previous, current, and next section are all collision tested
	size_t sectionIdx = std::min(std::max(0, (int)(p.pos.x / sectionSize)), (int)sections.size() - 1);
	auto prevSection = &sections[sectionIdx == 0 ? 0 : sectionIdx - 1];
	auto currSection = &sections[sectionIdx];
	auto nextSection = &sections[sectionIdx + 1 >= sections.size() - 1 ? sections.size() - 1 : sectionIdx + 1];

	// If at start or end of level, previous/next section is invalid so don't use it
	// NOTE: this used to compare &currSection against &prevSection, which are the
	// addresses of the local pointers and so never equal. At the first and last
	// section that made the same objects collide twice in one frame.
	std::vector<ObjectContainer>* sectionList[3] = { prevSection, nullptr, nullptr };
	if (currSection != prevSection)
		sectionList[1] = currSection;
	if (nextSection != currSection && nextSection != prevSection)
		sectionList[2] = nextSection;

	// Blocks are hazards processed separately
	std::vector<ObjectContainer> blocks;
	std::vector<ObjectContainer> hazards;
	blocks.reserve(100);
	hazards.reserve(100);

	size_t numCollisions = 0;

	for (auto section : sectionList) {
		if (section == nullptr) continue;
		for (auto& o : *section) {
			if (p.dead) break;
			if (o->prio == 1)
				blocks.push_back(o);
			else if (o->prio == 2)
				hazards.push_back(o);
			else if (o->touching(p)) {
				++numCollisions;
				o->collide(p);
			}
		}
	}

	// Blocks are processed in descending order
	for (int i = blocks.size() - 1; i >= 0; --i) {
		if (p.dead) break;
		auto& b = blocks[i];
		if (b->touching(p)) {
			++numCollisions;
			b->collide(p);
		}
	}

	for (auto& h : hazards) {
		if (p.dead) break;
		if (h->touching(p)) {
			++numCollisions;
			h->collide(p);
		}
	}

	if (!p.dead)
		p.postCollision();

	if (debug && !p.second) {
		std::cout << "Frame " << gameStates.size() << std::fixed << std::setprecision(8)
				  << " X " << p.pos.x << " Y " << p.pos.y - 15 << " Vel " << p.velocity
				  << " Accel " << p.acceleration << " Rot " << p.rotation << " Coll " << numCollisions
 				  << std::endl;

		if (p.button != gameStates.back().button) {
			std::cout << "Input X " << p.pos.x << " Y " << p.pos.y - 15 << std::endl;
		}
	}
}

Player& Level::runFrame(bool pressed, float dt) {
	Player p = gameStates.back();

	// Can't play if you're dead
	if (p.dead)
		return gameStates.back();

	bool wasDual = dual;
	Player p2;
	if (wasDual && !gameStates2.empty())
		p2 = gameStates2.back();

	stepPlayer(p, pressed, dt);

	if (wasDual && !gameStates2.empty()) {
		if (!p2.dead)
			stepPlayer(p2, pressed, dt);

		// The attempt ends when *either* player dies
		if (p2.dead)
			p.dead = true;

		gameStates2.push_back(p2);
	}

	// A dual portal was hit this frame: spawn the second player mirrored across it
	if (p.startDual && !dual) {
		Player mirror = p;
		mirror.second = true;
		mirror.pos.y = 2 * p.dualMirrorY - p.pos.y;
		mirror.upsideDown = !p.upsideDown;
		mirror.velocity = -p.velocity;
		mirror.grounded = false;
		mirror.startDual = false;
		mirror.usedEffects.clear();

		dual = true;
		dualStartFrame = p.frame;
		gameStates2.clear();
		gameStates2.push_back(mirror);
	}
	if (p.stopDual && dual) {
		dual = false;
		dualStartFrame = 0;
		gameStates2.clear();
	}

	p.startDual = false;
	p.stopDual = false;

	gameStates.push_back(p);
	return gameStates.back();
}


void Level::rollback(int frame) {
	gameStates.resize(frame > 0 ? frame : 1);

	if (dual) {
		int keep = currentFrame() - dualStartFrame + 1;
		if (keep <= 0) {
			// Rolled back past the dual portal entirely
			dual = false;
			dualStartFrame = 0;
			gameStates2.clear();
		} else if ((size_t)keep < gameStates2.size()) {
			gameStates2.resize(keep);
		}
	}
}

int Level::currentFrame() const {
	return gameStates.size();
}

Player const& Level::getState(int frame) const {
	if (frame == 0)
		return gameStates[0];
	if (gameStates.size() < (size_t)frame)
		return gameStates.back();
	return gameStates[frame - 1];
}

Player const& Level::getState(int frame, bool second) const {
	if (!second || gameStates2.empty())
		return getState(frame);

	int idx = frame - dualStartFrame;
	if (idx < 0)
		idx = 0;
	if ((size_t)idx >= gameStates2.size())
		idx = gameStates2.size() - 1;
	return gameStates2[idx];
}

Player& Level::latestState() {
	return gameStates.back();
}

Player const& Level::latestState() const {
	return gameStates.back();
}

bool Level::anyDead() const {
	if (gameStates.back().dead)
		return true;
	return dual && !gameStates2.empty() && gameStates2.back().dead;
}

std::optional<float> Level::spiderTarget(Player const& p) const {
	if (sections.empty())
		return std::nullopt;

	// The spider teleports to the nearest surface *overhead*, which is the
	// opposite of the gravity direction.
	int dir = p.upsideDown ? -1 : 1;
	Entity hb = p.unrotatedHitbox();

	float bestGap = std::numeric_limits<float>::max();
	std::optional<float> best;

	int idx = std::min(std::max(0, (int)(p.pos.x / sectionSize)), (int)sections.size() - 1);
	for (int s = idx - 1; s <= idx + 1; ++s) {
		if (s < 0 || s >= (int)sections.size())
			continue;

		for (auto const& o : sections[s]) {
			if (o->prio != 1)
				continue;
			if (o->getRight() <= hb.getLeft() || o->getLeft() >= hb.getRight())
				continue;

			float surface = dir > 0 ? o->getBottom() : o->getTop();
			float gap = dir > 0 ? surface - hb.getTop() : hb.getBottom() - surface;
			if (gap <= 0)
				continue;

			if (gap < bestGap) {
				bestGap = gap;
				best = surface - dir * (p.size.y / 2);
			}
		}
	}

	return best;
}
