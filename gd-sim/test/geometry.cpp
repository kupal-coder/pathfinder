#include <util.hpp>
#include <cassert>
#include <cmath>

int main() {
    // AABB overlap and separation.
    Entity player {{0, 0}, {10, 10}, 0};
    assert(player.intersects({{9, 0}, {10, 10}, 0}));
    assert(!player.intersects({{11, 0}, {10, 10}, 0}));

    // Regression: the old corner-based test reported false negatives when two
    // rotated rectangles crossed but no corner was inside the other rectangle.
    Entity horizontal {{0, 0}, {40, 4}, 45};
    Entity vertical {{0, 0}, {40, 4}, -45};
    assert(horizontal.intersects(vertical));

    // Rotation must affect collision; treating this object as an AABB would hit.
    Entity rotated {{10, 10}, {20, 2}, 45};
    Entity probe {{2, 18}, {2, 2}, 0};
    assert(!rotated.intersects(probe));

    auto manifold = player.collisionManifold({{0, -8}, {30, 10}, 0});
    assert(manifold);
    assert(manifold->normal.y > .99f);
    assert(std::abs(manifold->depth - 2.f) < .001f);
}
