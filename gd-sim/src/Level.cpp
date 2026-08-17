#include <sstream>
#include <iomanip>
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

	// Guard against a malformed kA4 indexing out of the speed tables.
	if (player.speed < 0 || player.speed > 4)
		player.speed = 1;

	if ((player.small = atoi(get_or("kA3", "0"))))
		player.size = player.size * 0.6;

	player.upsideDown = atoi(get_or("kA11", "0"));

	int vehicleId = atoi(get_or("kA2", "0"));
	if (vehicleId < 0 || vehicleId > 4)
		vehicleId = 0;
	player.vehicle = Vehicle::from(static_cast<VehicleType>(vehicleId));

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

		if (auto ob_o = Object::create(std::move(obj))) {
			auto ob = ob_o.value();

			// Unique ID
			ob->id = objectCount++;

			// Sections are divided by x position in increments of 100
			size_t sectionPos = std::max(.0f, ob->pos.x / sectionSize);
			if (sectionPos >= sections.size())
				sections.resize(sectionPos + 1);

			// Partition by collision priority once, here, instead of every frame.
			auto& section = sections[sectionPos];
			switch (ob->prio) {
				case 1:  section.blocks.push_back(ob); break;
				case 2:  section.hazards.push_back(ob); break;
				default: section.generic.push_back(ob); break;
			}

			if (ob->pos.x > length)
				length = ob->pos.x + 100;
		}
	}

	// A level with no recognised objects still has to be simulatable: the
	// collision loop indexes sections[idx +/- 1], which underflows on an empty
	// vector. One empty section keeps every index valid.
	if (sections.empty())
		sections.resize(1);

	player.level = this;
	history.reset(player);
}

void Level::setFullHistory(bool full) {
	Player initial = history.empty() ? Player() : history.at(1);
	initial.level = this;
	history.setCapacity(full ? 0 : StateHistory::kDefaultWindow);
	history.reset(initial);
}

Player& Level::runFrame(bool pressed, float dt) {
	Player p = history.back();

	// Can't play if you're dead
	if (p.dead)
		return history.back();

	p.dt = dt;
	p.preCollision(pressed);

	// Objects from previous, current, and next section are all collision tested
	int lastSection = static_cast<int>(sections.size()) - 1;
	int sectionIdx = std::min(std::max(0, (int)(p.pos.x / sectionSize)), lastSection);

	int prevIdx = sectionIdx > 0 ? sectionIdx - 1 : -1;
	int nextIdx = sectionIdx < lastSection ? sectionIdx + 1 : -1;

	// At the start or end of the level the neighbouring section does not exist.
	// The original guard compared the addresses of local pointers, which are
	// always distinct, so section 0 was scanned twice at the start of a level.
	Section const* active[3] = {
		prevIdx >= 0 ? &sections[prevIdx] : nullptr,
		&sections[sectionIdx],
		nextIdx >= 0 ? &sections[nextIdx] : nullptr,
	};

	size_t numCollisions = 0;

	// Generic objects first, in section order.
	for (auto const* section : active) {
		if (!section) continue;
		for (auto const& o : section->generic) {
			if (p.dead) break;
			if (o->touching(p)) {
				++numCollisions;
				o->collide(p);
			}
		}
	}

	// Blocks are processed in descending order, across the section window as a
	// whole -- matching the original, which flattened all three sections into
	// one list and walked it backwards.
	for (int s = 2; s >= 0; --s) {
		auto const* section = active[s];
		if (!section) continue;
		for (int i = static_cast<int>(section->blocks.size()) - 1; i >= 0; --i) {
			if (p.dead) break;
			auto const& b = section->blocks[i];
			if (b->touching(p)) {
				++numCollisions;
				b->collide(p);
			}
		}
	}

	for (auto const* section : active) {
		if (!section) continue;
		for (auto const& h : section->hazards) {
			if (p.dead) break;
			if (h->touching(p)) {
				++numCollisions;
				h->collide(p);
			}
		}
	}

	if (!p.dead)
		p.postCollision();

	if (debug) {
		std::cout << "Frame " << currentFrame() << std::fixed << std::setprecision(8)
				  << " X " << p.pos.x << " Y " << p.pos.y - 15 << " Vel " << p.velocity
				  << " Accel " << p.acceleration << " Rot " << p.rotation << " Coll " << numCollisions
 				  << std::endl;

		if (p.button != history.back().button) {
			std::cout << "Input X " << p.pos.x << " Y " << p.pos.y - 15 << std::endl;
		}
	}

	history.push(p);
	return history.back();
}


void Level::rollback(int frame) {
	history.truncate(frame);
}

int Level::currentFrame() const {
	return history.count();
}

Player const& Level::getState(int frame) const {
	return history.at(frame);
}

Player& Level::latestState() {
	return history.back();
}

Level::Checkpoint Level::checkpoint() const {
	return Checkpoint{history.checkpoint()};
}

void Level::restore(Checkpoint const& cp) {
	history.restore(cp.history);
	history.rebind(this);
}

void StateHistory::rebind(Level* owner) {
	for (auto& p : m_buf)
		p.level = owner;
}
