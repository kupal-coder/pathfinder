#pragma once
#include <algorithm>
#include <cmath>
#include <util.hpp>

/**
 * Oriented bounding box, and separating-axis intersection.
 *
 * The simulator used to approximate a rotated object with its axis-aligned
 * rect, and `Object::touching` only took rotation seriously at exact multiples
 * of 90 degrees:
 *
 *     int r = std::abs(rotation);
 *     return intersects((r == 0 || r == 90 || r == 180 || r == 270)
 *         ? player.unrotatedHitbox() : (Entity&)player);
 *
 * A block at 37 degrees was therefore tested against the wrong shape, which is
 * one of the ways a path that looks safe in the search kills you in game.
 *
 * The box is stored as a centre, half-extents and an angle, and tested with the
 * separating axis theorem. For two convex boxes SAT is exact: they intersect
 * iff no axis separates them, and only four axes need checking (two per box).
 */
struct OBB {
	Vec2D centre;
	Vec2D half;
	/// Radians, counter-clockwise.
	float angle = 0.f;

	OBB() = default;
	OBB(Vec2D c, Vec2D h, float a) : centre(c), half(h), angle(a) {}

	/// Axis-aligned box, the common case.
	static OBB aabb(Entity const& e) {
		return OBB({e.pos.x, e.pos.y}, {e.size.x / 2, e.size.y / 2}, 0.f);
	}

	static OBB from(Entity const& e, float radians) {
		return OBB({e.pos.x, e.pos.y}, {e.size.x / 2, e.size.y / 2}, radians);
	}

	bool axisAligned() const {
		// Within a thousandth of a degree of a right angle.
		float turns = angle / 1.5707963267948966f;
		return std::abs(turns - std::round(turns)) < 1e-5f;
	}

	/// The four corners, counter-clockwise from (-x, -y).
	void corners(Vec2D out[4]) const {
		float c = std::cos(angle);
		float s = std::sin(angle);

		Vec2D ax{c * half.x, s * half.x};   // local +x scaled
		Vec2D ay{-s * half.y, c * half.y};  // local +y scaled

		out[0] = {centre.x - ax.x - ay.x, centre.y - ax.y - ay.y};
		out[1] = {centre.x + ax.x - ay.x, centre.y + ax.y - ay.y};
		out[2] = {centre.x + ax.x + ay.x, centre.y + ax.y + ay.y};
		out[3] = {centre.x - ax.x + ay.x, centre.y - ax.y + ay.y};
	}

	/// Smallest axis-aligned box containing this one. Used for broad phase.
	Entity bounds() const {
		Vec2D c[4];
		corners(c);

		float minX = c[0].x, maxX = c[0].x, minY = c[0].y, maxY = c[0].y;
		for (int i = 1; i < 4; ++i) {
			minX = std::min(minX, c[i].x);
			maxX = std::max(maxX, c[i].x);
			minY = std::min(minY, c[i].y);
			maxY = std::max(maxY, c[i].y);
		}

		Entity e;
		e.pos = {(minX + maxX) / 2, (minY + maxY) / 2};
		e.size = {maxX - minX, maxY - minY};
		e.rotation = 0;
		return e;
	}
};

namespace obbdetail {

inline void project(Vec2D const corners[4], Vec2D axis, float& lo, float& hi) {
	lo = hi = corners[0].x * axis.x + corners[0].y * axis.y;
	for (int i = 1; i < 4; ++i) {
		float d = corners[i].x * axis.x + corners[i].y * axis.y;
		lo = std::min(lo, d);
		hi = std::max(hi, d);
	}
}

} // namespace obbdetail

/**
 * Separating axis test.
 *
 * Touching exactly counts as intersecting, matching the inclusive comparisons
 * the rest of the simulator uses for axis-aligned rects.
 */
inline bool intersects(OBB const& a, OBB const& b) {
	Vec2D ca[4], cb[4];
	a.corners(ca);
	b.corners(cb);

	float ac = std::cos(a.angle), as = std::sin(a.angle);
	float bc = std::cos(b.angle), bs = std::sin(b.angle);

	Vec2D axes[4] = {
		{ac, as}, {-as, ac},
		{bc, bs}, {-bs, bc},
	};

	for (auto axis : axes) {
		float alo, ahi, blo, bhi;
		obbdetail::project(ca, axis, alo, ahi);
		obbdetail::project(cb, axis, blo, bhi);
		if (ahi < blo || bhi < alo)
			return false;
	}
	return true;
}
