/**
 * Tests for oriented bounding box intersection.
 *
 * Rotated-object collision is one of the causes of "the search says safe, the
 * game says dead", so the geometry is worth testing on its own before anything
 * depends on it. Cases are chosen so the expected answer is obvious by hand.
 */
#include <OBB.hpp>
#include <cmath>
#include <iostream>
#include <string>

static int g_failures = 0;

static void check(bool cond, std::string const& what) {
	std::cout << (cond ? "  ok    " : "  FAIL  ") << what << "\n";
	if (!cond)
		++g_failures;
}

static constexpr float deg(float d) { return d * 3.14159265358979f / 180.f; }

int main() {
	std::cout << "-- oriented bounding boxes --\n";

	// Two unrotated unit boxes.
	OBB a({0, 0}, {5, 5}, 0);
	check(intersects(a, OBB({0, 0}, {5, 5}, 0)), "identical boxes intersect");
	check(intersects(a, OBB({9, 0}, {5, 5}, 0)), "overlapping boxes intersect");
	check(!intersects(a, OBB({11, 0}, {5, 5}, 0)), "separated boxes do not intersect");
	check(intersects(a, OBB({10, 0}, {5, 5}, 0)), "exactly touching counts as intersecting");

	// The diagonal case: a 45-degree square reaches further along the axes than
	// its unrotated form, so a gap that clears the AABB may not clear the OBB.
	OBB diamond({0, 0}, {5, 5}, deg(45));
	float reach = 5.f * std::sqrt(2.f);  // ~7.07
	check(intersects(diamond, OBB({reach - 0.1f, 0}, {0.05f, 0.05f}, 0)),
		  "45-degree box reaches its diagonal extent");
	check(!intersects(diamond, OBB({reach + 0.5f, 0}, {0.05f, 0.05f}, 0)),
		  "45-degree box stops past its diagonal extent");

	// Conversely, a rotated box does NOT fill its own bounding box corners.
	// This is exactly the case the old axis-aligned approximation got wrong:
	// a probe in the corner of the AABB is a false positive.
	check(!intersects(diamond, OBB({4.6f, 4.6f}, {0.05f, 0.05f}, 0)),
		  "rotated box does not fill its AABB corner (old code false-positived)");
	auto bb = diamond.bounds();
	check(std::abs(bb.size.x - 2 * reach) < 0.01f, "bounds of rotated box widen correctly");

	// A thin bar rotated across a gap: the classic spike-on-an-angle case.
	OBB bar({0, 0}, {20, 1}, deg(90));
	check(intersects(bar, OBB({0, 15}, {2, 2}, 0)), "vertical bar hits above its centre");
	check(!intersects(bar, OBB({15, 0}, {2, 2}, 0)), "vertical bar misses to the side");

	// Rotation by a full turn or by 180 degrees must not change the shape.
	check(intersects(OBB({0, 0}, {5, 2}, deg(180)), OBB({6, 0}, {2, 2}, 0)),
		  "180-degree rotation keeps extents");
	check(!intersects(OBB({0, 0}, {5, 2}, deg(90)), OBB({6, 0}, {2, 2}, 0)),
		  "90-degree rotation swaps extents");

	// axisAligned() drives a fast path, so it must be right at the boundaries.
	check(OBB({0, 0}, {1, 1}, 0).axisAligned(), "0 degrees is axis aligned");
	check(OBB({0, 0}, {1, 1}, deg(90)).axisAligned(), "90 degrees is axis aligned");
	check(OBB({0, 0}, {1, 1}, deg(270)).axisAligned(), "270 degrees is axis aligned");
	check(!OBB({0, 0}, {1, 1}, deg(45)).axisAligned(), "45 degrees is not axis aligned");
	check(!OBB({0, 0}, {1, 1}, deg(1)).axisAligned(), "1 degree is not axis aligned");

	// Agreement with the plain AABB path for unrotated boxes, over a sweep.
	int mismatches = 0;
	for (int dx = -20; dx <= 20; ++dx) {
		for (int dy = -20; dy <= 20; ++dy) {
			Entity e1{{0, 0}, {10, 10}, 0};
			Entity e2{{static_cast<float>(dx), static_cast<float>(dy)}, {10, 10}, 0};
			bool aabb = e1.intersects(e2);
			bool obb = intersects(OBB::aabb(e1), OBB::aabb(e2));
			if (aabb != obb)
				++mismatches;
		}
	}
	check(mismatches == 0, "unrotated OBB agrees with AABB across a 41x41 sweep");

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
