#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <utility>
#include <Level.hpp>
#include <Portals.hpp>

void Level::initLevelSettings(std::string const& lvlSettings, Player& player) {
	std::unordered_map<std::string, std::string> obj;
	std::stringstream ss2(lvlSettings);
	std::string k, v;
	while (std::getline(ss2, k, ',')) {
		if (!std::getline(ss2, v, ','))
			break;
		obj[k] = v;
	}

	// Return strings by value so defaults never expose pointers to temporary strings.
	auto get_or = [&obj](std::string const& key, std::string const& def) {
		if (auto it = obj.find(key); it != obj.end())
			return it->second;
		return def;
	};

	int speed = std::atoi(get_or("kA4", "0").c_str());
	// RobTop stores 1x speed as 0 and slow speed as 1.
	if (speed == 0)
		speed = 1;
	else if (speed == 1)
		speed = 0;
	player.speed = std::clamp(speed, 0, 4);

	player.size = {30.0f, 30.0f};
	player.small = std::atoi(get_or("kA3", "0").c_str()) != 0;
	if (player.small)
		player.size *= 0.6f;

	player.upsideDown = std::atoi(get_or("kA11", "0").c_str()) != 0;
	int vehicle = std::atoi(get_or("kA2", "0").c_str());
	if (vehicle < static_cast<int>(VehicleType::Cube) || vehicle > static_cast<int>(VehicleType::Spider))
		vehicle = static_cast<int>(VehicleType::Cube);
	player.vehicle = Vehicle::from(static_cast<VehicleType>(vehicle));
	player.floor = 0.0f;
	player.ceiling = player.vehicle.bounds;
}

Level::Level(std::string const& lvlString) {
	std::stringstream ss(lvlString);
	std::string objstr;
	bool first = true;
	auto player = Player();

	while (std::getline(ss, objstr, ';')) {
		if (first) {
			initLevelSettings(objstr, player);
			first = false;
			continue;
		}
		if (objstr.empty())
			continue;

		std::unordered_map<int, std::string> obj;
		std::stringstream ss2(objstr);
		std::string k, v;
		while (std::getline(ss2, k, ',')) {
			if (!std::getline(ss2, v, ','))
				break;
			int key = std::atoi(k.c_str());
			if (key > 0)
				obj[key] = v;
		}

		auto id = obj.find(1);
		if (id == obj.end() || id->second.empty())
			continue;

		if (id->second == "31") {
			initLevelSettings(objstr, player);
			player.pos.x = stod_def(obj[2], 0.0f);
			player.pos.y = stod_def(obj[3], 0.0f);
		}

		if (auto ob_o = Object::create(std::move(obj))) {
			auto ob = std::move(*ob_o);
			ob->id = static_cast<int>(objectCount++);
			size_t sectionPos = static_cast<size_t>(std::max(0.0f, ob->pos.x / static_cast<float>(sectionSize)));
			if (sectionPos >= sections.size())
				sections.resize(sectionPos + 1);
			if (ob->pos.x > length)
				length = ob->pos.x + 100.0f;
			sections[sectionPos].push_back(std::move(ob));
		}
	}

	player.level = this;
	gameStates.push_back(std::move(player));
	linkTeleportPortals();
}

Level::Level(Level const& other)
	: gameStates(other.gameStates), objectCount(other.objectCount), sections(other.sections),
	  length(other.length), debug(other.debug) {
	rebindInternalReferences();
}

Level::Level(Level&& other)
	: gameStates(std::move(other.gameStates)), objectCount(other.objectCount), sections(std::move(other.sections)),
	  length(other.length), debug(other.debug) {
	rebindInternalReferences();
}

Level& Level::operator=(Level const& other) {
	if (this == &other)
		return *this;
	gameStates = other.gameStates;
	objectCount = other.objectCount;
	sections = other.sections;
	length = other.length;
	debug = other.debug;
	rebindInternalReferences();
	return *this;
}

Level& Level::operator=(Level&& other) {
	if (this == &other)
		return *this;
	gameStates = std::move(other.gameStates);
	objectCount = other.objectCount;
	sections = std::move(other.sections);
	length = other.length;
	debug = other.debug;
	rebindInternalReferences();
	return *this;
}

Player& Level::runFrame(bool pressed, float dt) {
	Player p = gameStates.back();
	if (p.dead)
		return gameStates.back();

	p.dt = dt;
	p.preCollision(pressed);

	// Blocks and hazards are processed after normal effect objects.
	std::vector<Object const*> blocks;
	std::vector<Object const*> hazards;
	blocks.reserve(100);
	hazards.reserve(100);

	size_t numCollisions = 0;
	if (!sections.empty()) {
		size_t sectionIdx = static_cast<size_t>(std::clamp(
			static_cast<int>(p.pos.x / static_cast<float>(sectionSize)),
			0,
			static_cast<int>(sections.size()) - 1
		));
		size_t firstSection = sectionIdx == 0 ? 0 : sectionIdx - 1;
		size_t lastSection = std::min(sectionIdx + 1, sections.size() - 1);

		// Iterating an index range naturally visits boundary sections only once.
		for (size_t sectionIndex = firstSection; sectionIndex <= lastSection && !p.dead; ++sectionIndex) {
			for (auto const& container : sections[sectionIndex]) {
				auto const* object = container.operator->();
				if (object->prio == 1)
					blocks.push_back(object);
				else if (object->prio == 2)
					hazards.push_back(object);
				else if (object->touching(p)) {
					++numCollisions;
					object->collide(p);
				}
				if (p.dead)
					break;
			}
		}
	}

	for (auto it = blocks.rbegin(); it != blocks.rend() && !p.dead; ++it) {
		auto const* block = *it;
		if (block->touching(p)) {
			++numCollisions;
			block->collide(p);
		}
	}

	for (auto const* hazard : hazards) {
		if (p.dead)
			break;
		if (hazard->touching(p)) {
			++numCollisions;
			hazard->collide(p);
		}
	}

	if (!p.dead)
		p.postCollision();

	if (debug) {
		std::cout << "Frame " << gameStates.size() << std::fixed << std::setprecision(8)
				  << " X " << p.pos.x << " Y " << p.pos.y - 15 << " Vel " << p.velocity
				  << " Accel " << p.acceleration << " Rot " << p.rotation << " Coll " << numCollisions
				  << std::endl;
		if (p.button != gameStates.back().button)
			std::cout << "Input X " << p.pos.x << " Y " << p.pos.y - 15 << std::endl;
	}

	gameStates.push_back(std::move(p));
	return gameStates.back();
}

void Level::rollback(int frame) {
	gameStates.resize(frame > 0 ? static_cast<size_t>(frame) : 1);
}

int Level::currentFrame() const {
	return static_cast<int>(gameStates.size());
}

Player const& Level::getState(int frame) const {
	if (frame <= 0)
		return gameStates.front();
	if (static_cast<size_t>(frame) > gameStates.size())
		return gameStates.back();
	return gameStates[static_cast<size_t>(frame - 1)];
}

Player& Level::latestState() {
	return gameStates.back();
}

void Level::rebindInternalReferences() {
	for (auto& player : gameStates)
		player.level = this;
	linkTeleportPortals();
}

void Level::linkTeleportPortals() {
	std::unordered_map<int, std::vector<TeleportPortal*>> portalGroups;

	for (auto& section : sections) {
		for (auto& objContainer : section) {
			if (auto* portal = objContainer->asTeleportPortal()) {
				// A copied portal's old pointer refers to the source Level, so always clear it first.
				portal->linkedPortal = nullptr;
				if (portal->groupId >= 0)
					portalGroups[portal->groupId].push_back(portal);
			}
		}
	}

	for (auto& [groupId, portals] : portalGroups) {
		(void)groupId;
		if (portals.size() < 2)
			continue;
		std::sort(portals.begin(), portals.end(), [](TeleportPortal const* a, TeleportPortal const* b) {
			return a->pos.x < b->pos.x;
		});
		portals[0]->linkedPortal = portals[1];
		portals[1]->linkedPortal = portals[0];
	}
}
