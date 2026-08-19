#pragma once

#include <util.hpp>
#include <Vehicle.hpp>
#include <Slope.hpp>
#include <Physics.hpp>

#include <vector>
#include <functional>
#include <optional>

// Must be after all includes to prevent MSVC / Windows RPC macro collisions
#ifdef small
#undef small
#endif

// Backward compatibility aliases (to be phased out)
inline constexpr const float (&player_speeds)[5] = PHYS_SPEEDS;
inline constexpr const float (&player_speedmults)[5] = PHYS_SPEED_MULTS;

float roundVel(float velocity, bool upsideDown);

struct Object;
class Level;

/**
 * The main player. This contains the entire player state and is the only thing that changes each frame.
 * All physics fields use float to match Geometry Dash's internal 32-bit precision.
 */
struct Player : public Entity {
        Vehicle vehicle         = Vehicle::Cube;
        Level* level            = nullptr;
        
        float timeElapsed       = 0.0f;
        float acceleration      = 0.0f;
        float velocity          = 0.0f;
        float ceiling           = 0.0f;
        float floor             = 0.0f;
        float dt                = PHYS_DT;

        /// See util.hpp for what cow_set is
        cow_set<int> usedEffects;

        // Potential slopes are important for block collisions. Reset per frame.
        std::vector<Slope const*> potentialSlopes;

        /// Actions will be run at the beginning of every frame.
        std::vector<std::function<void(Player&)>> actions;

        /// Slopes have special collision rules. See Slope.cpp for more information.
        struct {
                std::optional<Slope> slope;
                /// Time on slope
                float elapsed = 0.0f;
                /// When colliding with a downhill slope with a positive velocity.
                bool snapDown = false;
        } slopeData;

        /// X-snapping. See Block.cpp for more information
        struct {
                Entity object;
                int playerFrame = 0;
        } snapData;

        /// Some vehicles have coyote frames for valid inputs
        unsigned int coyoteFrames = 0;

        /// Robot mode: Remaining boost ticks while holding jump (GD uses ~85 ticks at 240Hz, not air jumps)
        int robotBoostTicks       = 0;

        int speed                 = 1; // Default 1x speed
        int frame                 = 0;

        // --- Boolean flags (packed bitfield for cache efficiency) ---
        bool dead             : 1 = false;
        bool grounded         : 1 = false;

        /**
         * Under normal circumstances, acceleration is applied to the velocity
         * at the end of a frame. When this field is set, acceleration will not
         * be applied at the end of the frame. Mainly set via `setVelocity`
         */
        bool velocityOverride : 1 = false;

        /**
         * `button` vs `input`: Button always refers to whether a click was applied; input can be disabled
         * for niche circumstances where no other normal operations are allowed despite a click.
         */
        bool button           : 1 = false;
        bool input            : 1 = false;

        /// If a click is being buffered. Used for things like orb clicks.
        bool buffer           : 1 = false;

        /**
         * In the ball vehicle, holding a click while transitioning into another vehicle will cause a
         * buffered input, despite things like orbs not buffering in the same way.
         */
        bool vehicleBuffer    : 1 = false;

        bool upsideDown       : 1 = false;
        bool small            : 1 = false;

        /// Entering a gravity portal can cause the next frame to have certain edge cases
        bool gravityPortal    : 1 = false;

        /// Whether velocity at the end of the frame is to be rounded (typically used by ball)
        bool roundVelocity    : 1 = false;

        /// Robot mode: Whether the initial ground jump was initiated and button is held
        bool isRobotBoosting  : 1 = false;

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
        inline float gravTop(Entity const& e) const    { return upsideDown ? -e.getBottom() : e.getTop(); }
        inline float gravFloor() const                 { return upsideDown ? -ceiling : floor; }
        inline float gravCeiling() const               { return upsideDown ? -floor : ceiling; }

        void setVelocity(float v, bool override = false);
};