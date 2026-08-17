#pragma once
#undef small
#include <util.hpp>
#include <Vehicle.hpp>
#include <Slope.hpp>
#include <Physics.hpp>
#include <vector>
#include <functional>
#include <optional>

// Backward compatibility aliases (to be phased out)
inline constexpr const float (&player_speeds)[5] = PHYS_SPEEDS;
inline constexpr const float (&player_speedmults)[5] = PHYS_SPEED_MULTS;

float roundVel(float velocity, bool upsideDown);

struct Object;
class Level;
struct Slope;

/**
 * The main player. This contains the entire player state and is the only thing that changes each frame.
 * All physics fields use float to match Geometry Dash's internal 32-bit precision.
 */
struct Player : public Entity {
	Vehicle vehicle;
	Level* level;
	float timeElapsed;
	float acceleration;
	float velocity;

	/// See util.hpp for what cow_set is
	cow_set<int> usedEffects;

	// Potential slopes are important for block collisions. Reset per frame.
	std::vector<Slope const*> potentialSlopes;

	/// Actions will be ran at the beginning of every frame.
	std::vector<std::function<void(Player&)>> actions;

	/// Slopes have special collision rules. See Slope.cpp for more information.
	struct {
		std::optional<Slope> slope;
		/// Time on slope
		float elapsed;
		/// When colliding with a downhill slope with a positive velocity.
		bool snapDown;
	} slopeData;

	/// X-snapping. See Block.cpp for more information
	struct {
		Entity object;
		int playerFrame = 0;
	} snapData;

	float ceiling;
	float floor;
	float dt;

	/// Some vehicles have coyote frames for valid inputs
	unsigned int coyoteFrames;

	/// Robot mode: remaining air jumps available
	unsigned int jumpsRemaining;

	int speed;
	int frame;

	// --- Boolean flags (packed for cache efficiency) ---
	bool dead : 1;
	bool grounded : 1;

	/**
	 * Under normal circumstances, acceleration is applied to the velocity
	 * at the end of a frame. When this field is set, acceleration will not
	 * be applied at the end of the frame. Mainly set via `setVelocity`
	 */
	bool velocityOverride : 1;

	/**
	 * `button` vs `input`: Button always refers to whether a click was applied; input can be disabled
	 * for niche circumstances where no other normal operations are allowed despite a click.
	 */
	bool button : 1;
	bool input : 1;

	/// If a click is being buffered. Used for things like orb clicks.
	bool buffer : 1;

	/**
	 * In the ball vehicle, holding a click while transitioning into another vehicle will cause a
	 * buffered input, despite things like orbs not buffering in the same way.
	 */
	bool vehicleBuffer : 1;

	bool upsideDown : 1;
	bool small : 1;

	/// Entering a gravity portal can cause the next frame to have certain edge cases
	bool gravityPortal : 1;

	/// Whether velocity at the end of the frame is to be rounded (typically used by ball)
	bool roundVelocity : 1;

	Player();

	void preCollision(bool input);
	void postCollision();
	
	Entity unrotatedHitbox() const;

	/// Inner hitbox of {9, 9} that is used mainly for blocks
	Entity innerHitbox() const;

	Player const& prevPlayer() const;
	Player const* nextPlayer() const;

	/// Values relative to player gravity.
	template <typename T>
	T grav(T value) const { return upsideDown ? -value : value; }

	inline float gravBottom(Entity const& e) const { return upsideDown ? -e.getTop() : e.getBottom(); }
	inline float gravTop(Entity const& e) const { return upsideDown ? -e.getBottom() : e.getTop(); }
	inline float gravFloor() const { return upsideDown ? -ceiling : floor; }
	inline float gravCeiling() const { return upsideDown ? -floor : ceiling; }

	void setVelocity(float v, bool override = false);
};
