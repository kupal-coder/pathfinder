#include <Slope.hpp>
#include <cmath>
#include <algorithm>
#include <Player.hpp>
#include <Physics.hpp>

/// Slopes are possibly the most complicated object. This is still very unfinished!
Slope::Slope(Vec2D size, std::unordered_map<int, std::string>&& fields) : Block(size, std::move(fields)) {
	auto rot = stod_def(fields[6].c_str());
	bool flipX = atoi(fields[4].c_str()) == 1;
	bool flipY = atoi(fields[5].c_str()) == 1;
	orientation = static_cast<int>(rot) / 90;

	// Slope orientation matters a lot
	if (flipX && flipY)
		orientation += 2;
	else if (flipX)
		orientation += 1;
	else if (flipY)
		orientation += 3;

	orientation = orientation % 4;
	if (orientation < 0) orientation += 4;
	rotation = 0;
}

bool Slope::isFacingUp() const {
	return orientation < 2;
}

int Slope::gravOrient(Player const& p) const {
	int orient = orientation;
	if (p.upsideDown) {
		if (orient == 3)
			orient = 0;
		else if (orient == 2)
			orient = 1;
		else if (orient == 0)
			orient = 3;
		else if (orient == 1)
			orient = 2;
	}
	return orient;
}

float Slope::angle() const {
	auto ang = std::atan(size.y / size.x);
	// Downhill slopes 
	if (orientation == 1 || orientation == 3)
		ang = -ang;
	return ang;
}

float Slope::expectedY(Player const& p) const {
	float ydist = (isFacingUp() ? 1.0f : -1.0f) * (p.size.y / 2.0f) / std::cos(angle());
	float posRelative = (size.y / size.x) * (p.pos.x - getLeft());

	// Uphill vs downhill
	if (angle() > 0.0f)
		return getBottom() + std::min(posRelative + ydist, size.y + p.size.y / 2.0f);
	else
		return getTop() - std::min(posRelative - ydist, size.y + p.size.y / 2.0f);
}

/// See Slope.hpp for why this is a separate function
void Slope::calc(Player& p) const {
	if (gravOrient(p.prevPlayer()) == 0) {
		// Regular uphill slope
		// Coyote frame for slopes must be taken into account
		if (!touching(p)) {
			p.actions.push_back(+[](Player& p) {
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
		}

		// If player isn't on top already, use expectedY to snap player
		if (p.gravBottom(p.prevPlayer()) != getTop()) {
			if (p.prevPlayer().upsideDown) {
				p.pos.y = std::min(p.pos.y, expectedY(p));
			} else {
				p.pos.y = std::max(p.pos.y, expectedY(p));
			}
		}

		// When you're on top of the slope you will be ejected
		if (p.grounded && (p.gravBottom(p) == p.gravTop(*this) || (p.gravBottom(p) > p.gravTop(*this) && p.snapData.playerFrame > 0))) {
			
			// Rob's algorithm for slope ejection velocity. So goofy!
			float vel = 0.9f * std::min(1.12f / p.grav(angle()), 1.54f) * (size.y * PHYS_SPEEDS[p.speed] / size.x);
			float time = std::clamp(10.0f * (p.timeElapsed - p.slopeData.elapsed), 0.4f, 1.0f);

			if (p.vehicle.type == VehicleType::Ball || p.vehicle.type == VehicleType::Ship)
				vel *= 0.75f;
			if (p.vehicle.type == VehicleType::Ufo)
				vel *= 0.7499f; // I have no justification for this. It just works

			vel *= time;

			// Gotta eject on the next frame
			p.actions.push_back([vel](Player& p) {
				p.velocity = roundVel(vel, p.upsideDown);
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
		}
	} else if (gravOrient(p.prevPlayer()) == 1) {
		// Downhill regular slope
		// Velocity up means you're not on slope anymore
		if (p.velocity > 0.0f) {
			p.actions.push_back(+[](Player& p) {
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
		}

		// Snap to expected Y just like uphill
		if (p.gravBottom(p.prevPlayer()) != getTop() || p.slopeData.snapDown) {
			p.pos.y = std::max(p.pos.y, expectedY(p.prevPlayer()));
			p.pos.y = std::max(std::min(p.pos.y, expectedY(p)), pos.y - p.size.y / 2.0f);
		}

		// Ejections, but downwards!
		if (p.getTop() <= pos.y) {
			static constexpr float falls[4] = {
				226.044054f,
				280.422108f,
				348.678108f,
				421.200108f
			};
			float vel = -falls[p.speed] * (size.y / size.x);
			p.velocity = 0.0f;

			p.actions.push_back([vel](Player& p) {
				p.velocity = vel;
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
		}
	} else if (gravOrient(p.prevPlayer()) == 2) {
		if (p.velocity < 0.0f) {
			p.actions.push_back(+[](Player& p) {
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
			return;
		}

		p.velocity = 0.0f;

		if (p.grav(p.pos.y) < p.gravTop(*this)) {
			p.pos.y = p.grav(std::max(p.grav(p.pos.y), p.grav(expectedY(p))));
		}

		if (p.grav(p.pos.y) >= p.gravTop(*this)) {
			p.pos.y = p.grav(p.gravTop(*this));
			p.velocity = roundVel(p.prevPlayer().acceleration * p.dt, p.prevPlayer().upsideDown);
			p.actions.push_back(+[](Player& p) {
				p.slopeData.slope = {};
				p.slopeData.elapsed = 0.0f;
				p.slopeData.snapDown = false;
			});
		}
	}
}

void Slope::collide(Player& p) const {
	p.potentialSlopes.push_back(this);

	if (orientation < 2 && expectedY(p) <= p.pos.y)
		return;
	else if (orientation >= 2 && expectedY(p) >= p.pos.y)
		return;
	else if (p.vehicle.type == VehicleType::Cube && p.gravTop(p) - p.gravBottom(*this) < 16.0f)
		return;

	// No slope calculations for you!
	if (p.vehicle.type == VehicleType::Wave) {
		p.dead = true;
		return;
	}

	// When you hit a downhill slope before your center hits the leftmost side, it's treated like a block
	if (!p.prevPlayer().slopeData.slope && gravOrient(p) == 1 && p.velocity <= 0.0f && p.pos.x - getLeft() < 0.0f) {
		p.pos.y = p.grav(p.gravTop(*this) + p.size.y / 2.0f);
		p.grounded = true;
		return;
	}

	if (!p.prevPlayer().slopeData.slope && gravOrient(p) == 2 && p.velocity >= 0.0f && p.pos.x - getLeft() < 0.0f) {
		p.pos.y = p.grav(p.gravBottom(*this) - p.size.y / 2.0f);
		p.velocity = 0.0f;
		return;
	}

	// Current (or previous) slope
	auto pSlope = p.slopeData.slope;

	/*
		If stored slope data is current slope, or there is no stored,
		or you're no longer touching the previous slope.
	*/
	if (!pSlope || !pSlope->touching(p) || (pSlope->gravOrient(p) == gravOrient(p) && p.grav(expectedY(p)) > p.grav(pSlope->expectedY(p))) || pSlope->id == id) {
		bool hasSlope = p.prevPlayer().slopeData.slope.has_value();

		// Is player traveling at the right angle to contact the slope
		float pAngle = std::atan((p.prevPlayer().velocity * p.dt) / (PHYS_SPEEDS[p.speed] * p.dt));
		if (gravOrient(p.prevPlayer()) > 1)
			pAngle = -pAngle;

		bool projectedHit = orientation == 1 ? (pAngle * 5.0f <= angle()) : (pAngle <= angle());

		// Downhill slopes snap you down
		bool snapDown = orientation == 1 && p.velocity > 0.0f && p.pos.x - getLeft() > 0.0f;

		if (hasSlope ? p.velocity <= 0.0f : (projectedHit || snapDown)) {
			p.grounded = true;
			p.slopeData.slope = *this;

			if (snapDown && !hasSlope) {
				p.velocity = 0.0f;
				p.pos.y = getTop() + p.size.y / 2.0f;
				p.slopeData.snapDown = true;
			}

			if (p.slopeData.elapsed == 0.0f) 
				p.slopeData.elapsed = p.prevPlayer().timeElapsed;
		}
	}
}

void SlopeHazard::collide(Player& p) const {
	if (orientation < 2 && expectedY(p) <= p.pos.y)
		return;
	else if (orientation >= 2 && expectedY(p) >= p.pos.y)
		return;
	p.dead = true;
}

float SlopeHazard::expectedY(Player const& p) const {
	// Hazardous slopes have slightly larger hitboxes
	return Slope::expectedY(p) + (orientation > 1 ? -4.0f : 4.0f);
}

bool SlopeHazard::touching(Player const& p) const {
	Entity hitbox = p.unrotatedHitbox();
	hitbox.size.y += 8.0f;

	if (!intersects(hitbox))
		return false;

	switch (orientation) {
		case 0:
			return expectedY(p) > p.pos.y;
		case 1:
			return expectedY(p) > p.pos.y;
		case 2:
			return expectedY(p) < p.pos.y;
		case 3:
			return expectedY(p) < p.pos.y;
		default:
			return false;
	}
}
