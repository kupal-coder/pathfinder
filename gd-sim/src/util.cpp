#include <util.hpp>
#include <cmath>
#include <complex>
#include <array>
#include <limits>
#include <algorithm>

float slerp(float fromAngle, float toAngle, float t) {
    std::complex<float> fromVec = std::polar(1.0f, fromAngle * 0.5f);
    std::complex<float> toVec = std::polar(1.0f, toAngle * 0.5f);


    float dot = std::imag(fromVec) * std::imag(toVec) + std::real(fromVec) * std::real(toVec);
    if (dot < 0.0f) {
        dot *= -1;
        toVec *=  -1;
    }

    std::complex<float> weight = std::complex(1.0f - t, t);
    if (dot < 0.9999) {
        float between = std::acos(dot);

        weight *= between;
        weight = std::complex(std::sin(weight.real()), std::sin(weight.imag()));
        weight /= std::sin(between);
    }

    std::complex<float> interpVec = (weight.imag() * toVec) + (weight.real() * fromVec);
    return std::atan2(std::imag(interpVec), std::real(interpVec)) * 2;
}


Vec2D Vec2D::rotate(float angle, Vec2D const& pivot) const {
    if (angle == 0) return *this;

    Vec2D tmp = *this - pivot;

    float rad = deg2rad(angle);
    float s = std::sin(rad);
    float c = std::cos(rad);

    tmp = {tmp.x * c - tmp.y * s, tmp.x * s + tmp.y * c};
    tmp += pivot;

    return tmp;
}

namespace {

struct Projection {
    float min;
    float max;
};

std::array<Vec2D, 4> corners(Entity const& entity) {
    const float hx = entity.size.x * .5f;
    const float hy = entity.size.y * .5f;
    std::array<Vec2D, 4> result {{
        {-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}
    }};
    for (auto& point : result)
        point = (point + entity.pos).rotate(entity.rotation, entity.pos);
    return result;
}

Vec2D normalized(Vec2D vector) {
    const float length = std::hypot(vector.x, vector.y);
    return length == 0 ? Vec2D{} : vector / length;
}

Projection project(std::array<Vec2D, 4> const& points, Vec2D axis) {
    Projection result {
        points[0].x * axis.x + points[0].y * axis.y,
        points[0].x * axis.x + points[0].y * axis.y
    };
    for (size_t i = 1; i < points.size(); ++i) {
        const float value = points[i].x * axis.x + points[i].y * axis.y;
        result.min = std::min(result.min, value);
        result.max = std::max(result.max, value);
    }
    return result;
}

} // namespace

std::optional<CollisionManifold> Entity::collisionManifold(Entity const& other) const {
    // A cheap conservative rejection keeps SAT out of the hot path for distant
    // objects.  Half diagonals are valid for every orientation.
    const float radiusA = .5f * std::hypot(size.x, size.y);
    const float radiusB = .5f * std::hypot(other.size.x, other.size.y);
    if (std::abs(pos.x - other.pos.x) > radiusA + radiusB ||
        std::abs(pos.y - other.pos.y) > radiusA + radiusB)
        return {};

    const auto a = corners(*this);
    const auto b = corners(other);
    const std::array<Vec2D, 4> axes {{
        normalized({a[1].x - a[0].x, a[1].y - a[0].y}),
        normalized({a[3].x - a[0].x, a[3].y - a[0].y}),
        normalized({b[1].x - b[0].x, b[1].y - b[0].y}),
        normalized({b[3].x - b[0].x, b[3].y - b[0].y})
    }};

    float leastDepth = std::numeric_limits<float>::max();
    Vec2D leastAxis;
    for (auto axis : axes) {
        const auto pa = project(a, axis);
        const auto pb = project(b, axis);
        const float overlap = std::min(pa.max, pb.max) - std::max(pa.min, pb.min);
        // Edge contact is a collision in GD. It matters for landing exactly on
        // a rotated surface, so only a negative overlap separates the boxes.
        if (overlap < 0)
            return {};
        if (overlap < leastDepth) {
            leastDepth = overlap;
            leastAxis = axis;
        }
    }

    const Vec2D centerDelta = pos - other.pos;
    if (centerDelta.x * leastAxis.x + centerDelta.y * leastAxis.y < 0)
        leastAxis *= -1;
    return CollisionManifold {leastAxis * leastDepth, leastAxis, leastDepth};
}

bool Entity::intersects(Entity const& other) const {
    return collisionManifold(other).has_value();
}

/*using Vec2 = Vec2D;
// Return normalized perpendicular axis from two points
static Vec2 edgeToAxis(const Vec2D& p1, const Vec2& p2) {
    Vec2 edge = {p2.x - p1.x, p2.y - p1.y};
    Vec2 axis = {-edge.y, edge.x}; // perpendicular

    float len = std::sqrt(axis.x * axis.x + axis.y * axis.y);
    if (len != 0.0f) {
        axis.x /= len;
        axis.y /= len;
    }
    return axis;
}

// Compute the four corners of a rectangle
static void getCorners(const Entity& e, Vec2 outCorners[4]) {
    float hw = e.size.x * 0.5f;
    float hh = e.size.y * 0.5f;
    float rad = deg2rad(e.rotation);

    float cosA = std::cos(rad);
    float sinA = std::sin(rad);

    // Local unrotated corners
    Vec2 local[4] = {
        {-hw, -hh},
        { hw, -hh},
        { hw,  hh},
        {-hw,  hh}
    };

    // Rotate + translate
    for (int i = 0; i < 4; ++i) {
        outCorners[i].x = e.pos.x + (local[i].x * cosA - local[i].y * sinA);
        outCorners[i].y = e.pos.y + (local[i].x * sinA + local[i].y * cosA);
    }
}

// Project rectangle corners onto an axis
static void projectOntoAxis(const Vec2 corners[4], const Vec2& axis,
                            float& min, float& max) {
    min = max = corners[0].x * axis.x + corners[0].y * axis.y;
    for (int i = 1; i < 4; ++i) {
        float projection = corners[i].x * axis.x + corners[i].y * axis.y;
        if (projection < min) min = projection;
        if (projection > max) max = projection;
    }
}

// ------------------------------------------------------------
// Entity::collidesWith
// ------------------------------------------------------------
bool Entity::intersects(const Entity& other) const {
    Vec2 cornersA[4];
    Vec2 cornersB[4];
    getCorners(*this, cornersA);
    getCorners(other, cornersB);

    // Axes to test: 2 unique from each rectangle
    Vec2 axes[4] = {
        edgeToAxis(cornersA[0], cornersA[1]),
        edgeToAxis(cornersA[1], cornersA[2]),
        edgeToAxis(cornersB[0], cornersB[1]),
        edgeToAxis(cornersB[1], cornersB[2]),
    };

    // SAT check: if projections do NOT overlap on any axis -> no collision
    for (int i = 0; i < 4; ++i) {
        float minA, maxA, minB, maxB;
        projectOntoAxis(cornersA, axes[i], minA, maxA);
        projectOntoAxis(cornersB, axes[i], minB, maxB);

        if (maxA < minB || maxB < minA)
            return false; // Found separating axis
    }

    return true; // No separating axis found -> collision
}*/


