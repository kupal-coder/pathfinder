#include <Orb.hpp>
#include <Player.hpp>
#include <Physics.hpp>
#include <algorithm>

Orb::Orb(Vec2D size, std::unordered_map<int, std::string>&& fields) : EffectObject(size, std::move(fields)) {
	switch (std::stoi(fields[1])) {
		case 36:
			type = OrbType::Yellow;
			break;
		case 84:
			type = OrbType::Blue;
			break;
		case 141:
			type = OrbType::Pink;
			break;
		case 1333:
			type = OrbType::Red;
			break;
		case 1330:
			type = OrbType::Black;
			break;
		case 1022:
			type = OrbType::Green;
			break;
		case 1704:
		case 1751:
			type = OrbType::Dash;
			break;
		default:
			type = OrbType::Yellow;
			break;
	};
}

bool Orb::touching(Player const& p) const {
	// Coyote frame for orbs
	return EffectObject::touching(p) || EffectObject::touching(p.prevPlayer());
}

namespace {
VehicleType orbPhysicsVehicle(VehicleType type) {
	// Robot and Spider use the cube's orb impulse values.
	if (type == VehicleType::Robot || type == VehicleType::Spider)
		return VehicleType::Cube;
	return type;
}
}

const velocity_map<OrbType, VehicleType, bool> orb_velocities = {
	//											   slow speed   1x speed    2x speed   3x speed
	{{OrbType::Yellow, VehicleType::Cube, false}, {573.48f,     603.72f,    616.68f,   606.42f}},
	{{OrbType::Yellow, VehicleType::Cube, true},  {458.784f,    482.976f,   481.734f,  485.136f}},
	{{OrbType::Yellow, VehicleType::Ship, false}, {573.48f,     603.72f,    616.68f,   606.42f}},
	{{OrbType::Yellow, VehicleType::Ship, true},  {458.784f,    482.976f,   481.734f,  485.136f}},
	{{OrbType::Yellow, VehicleType::Ball, false}, {401.435993f, 422.60399f, 431.67599f,424.493993f}},
	{{OrbType::Yellow, VehicleType::Ball, true},  {321.148795f, 338.08319f, 345.34079f,339.59519f}},
	{{OrbType::Yellow, VehicleType::Ufo, false},  {573.48f,     603.72f,    616.68f,   606.42f}},
	{{OrbType::Yellow, VehicleType::Ufo, true},   {458.784f,    482.976f,   481.734f,  485.136f}},

	{{OrbType::Blue, VehicleType::Cube, false},   {-229.392f,   -241.488f,  -246.672f, -242.568f}},
	{{OrbType::Blue, VehicleType::Cube, true},    {-183.519f,   -193.185f,  -197.343f, -194.049f}},
	{{OrbType::Blue, VehicleType::Ship, false},   {-229.392f,   -241.488f,  -246.672f, -242.568f}},
	{{OrbType::Blue, VehicleType::Ship, true},    {-183.519f,   -193.185f,  -197.343f, -194.049f}},
	{{OrbType::Blue, VehicleType::Ball, false},   {-160.574397f,-169.04160f, -172.6704f,-169.7976f}},
	{{OrbType::Blue, VehicleType::Ball, true},    {-128.463298f,-135.2295f,  -138.1401f,-135.8343f}},
	{{OrbType::Blue, VehicleType::Ufo, false},    {-229.392f,   -241.48f,   -246.672f, -242.568f}},
	{{OrbType::Blue, VehicleType::Ufo, true},     {-183.519f,   -193.185f,  -197.343f, -194.049f}},

	{{OrbType::Pink, VehicleType::Cube, false},   {412.884f,    434.7f,     443.988f,  436.644f}},
	{{OrbType::Pink, VehicleType::Cube, true},    {330.318f,    347.76f,    355.212f,  349.272f}},
	{{OrbType::Pink, VehicleType::Ship, false},   {212.166f,    223.398f,   228.15f,   224.37f}},
	{{OrbType::Pink, VehicleType::Ship, true},    {169.776f,    178.686f,   182.52f,   179.496f}},
	{{OrbType::Pink, VehicleType::Ball, false},   {309.090595f, 325.42019f, 332.37539f,326.85659f}},
	{{OrbType::Pink, VehicleType::Ball, true},    {247.287596f, 260.3286f,  265.923f,  261.5004f}},
	{{OrbType::Pink, VehicleType::Ufo, false},    {240.84f,     253.584f,   258.984f,  254.718f}},
	{{OrbType::Pink, VehicleType::Ufo, true},     {192.672f,    202.824f,   207.198f,  203.742f}},

	{{OrbType::Red, VehicleType::Cube, false},    {779.976f,    821.448f,   839.43f,   825.174f}},
	{{OrbType::Red, VehicleType::Cube, true},     {621.702f,    654.858f,   669.222f,  657.828f}},
	{{OrbType::Red, VehicleType::Ship, false},    {569.754f,    599.994f,   612.954f,  602.694f}},
	{{OrbType::Red, VehicleType::Ship, true},     {637.902f,    671.814f,   686.286f,  674.838f}},
	{{OrbType::Red, VehicleType::Ball, false},    {530.928f,    559.278f,   571.482f,  561.816f}},
	{{OrbType::Red, VehicleType::Ball, true},     {423.36f,     446.04f,    455.76f,   448.092f}},
	{{OrbType::Red, VehicleType::Ufo, false},     {577.962f,    608.85f,    622.026f,  611.604f}},
	{{OrbType::Red, VehicleType::Ufo, true},      {615.762f,    648.648f,   662.742f,  651.564f}},

	{{OrbType::Green, VehicleType::Cube, false},  {562.032f,    592.056f,   605.07f,   594.756f}},
	{{OrbType::Green, VehicleType::Cube, true},   {447.336f,    471.312f,   481.734f,  485.136f}},
	{{OrbType::Green, VehicleType::Ship, false},  {406.08f,     427.248f,   432.0f,    429.138f}},
	{{OrbType::Green, VehicleType::Ship, true},   {326.592f,    343.548f,   350.784f,  345.06f}},
	{{OrbType::Green, VehicleType::Ball, false},  {394.47f,     415.638f,   424.71f,   417.528f}},
	{{OrbType::Green, VehicleType::Ball, true},   {314.172f,    331.074f,   331.074f,  332.586f}},
	{{OrbType::Green, VehicleType::Ufo, false},   {432.0f,      432.0f,     432.0f,    432.0f}},
	{{OrbType::Green, VehicleType::Ufo, true},    {450.576f,    474.768f,   485.136f,  476.928f}},
};

void Orb::collide(Player& p) const {
	// Orbs are often buffered, but p.buffer will still be true if its not a real buffer
	if (p.buffer || (p.prevPlayer().buffer && !p.button) || (p.vehicle.type == VehicleType::Ball && p.vehicleBuffer)) {
		p.buffer = false;
		p.vehicleBuffer = false;

		EffectObject::collide(p);

		if (type == OrbType::Dash) {
			// Dash orb: directional velocity boost based on orb rotation
			// Default (0°): upward boost; rotation determines direction
			float rad = deg2rad(rotation);
			constexpr float dashSpeed = 580.0f;
			float vy = dashSpeed * std::cos(rad);
			p.velocity = p.grav(vy);
			p.grounded = false;
			p.velocityOverride = true;
		} else if (p.vehicle.type != VehicleType::Wave) {
			// Wave can't use non-gravity orbs
			if (type == OrbType::Black) {
				p.velocity = -810.0f;
			} else {
				p.velocity = orb_velocities.get(type, orbPhysicsVehicle(p.vehicle.type), p.small, std::clamp(p.speed, 0, 3));
				p.grounded = false;
				if (type == OrbType::Green) {
					p.velocityOverride = true;
				}
			}
		}

		if (type == OrbType::Blue || type == OrbType::Green) {
			p.upsideDown = !p.upsideDown;
		}

		// Clicking on an orb as ball essentially removes the input
		if (p.vehicle.type == VehicleType::Ball)
			p.input = false;
	}
}
