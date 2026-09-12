#include <Object.hpp>
#include <Player.hpp>
#include <Level.hpp>
#include <string>
#include <cmath>
#include <exception>

#include <Block.hpp>
#include <Hazard.hpp>
#include <Portals.hpp>
#include <Pad.hpp>
#include <Orb.hpp>
#include <Slope.hpp>


/// Helper classes and functions to make assigning object IDs easier
struct range : public std::pair<int, int> {
	range(int i) : std::pair<int, int>({i, i}) {}
	range(int i, int j) : std::pair<int, int>({i, j}) {}
};

std::vector<int> unroll(std::vector<range> ranges) {
	std::vector<int> res;
	for (auto r : ranges)
		for (int i = r.first; i <= r.second; i++)
			res.push_back(i);
	return res;
};

#define objs(x, type, w, h) \
	for (auto& i : unroll x) \
		if (id == i) \
			return ObjectContainer(type({w, h}, std::move(ob)));

std::optional<ObjectContainer> Object::create(std::unordered_map<int, std::string>&& ob) {
	auto idIt = ob.find(1);
	if (idIt == ob.end())
		return {};

	int id = 0;
	try {
		id = std::stoi(idIt->second);
	} catch (std::exception const&) {
		return {};
	}

	objs(({
		{1, 4}, {6, 7}, 63, {69, 72},
		{74, 78}, {81, 83}, {90, 96},
		{116, 119}, {121, 122}, 146,
		{160, 163}, {165, 169}, 173,
		175, {207, 210}, {212, 213},
		{247, 250}, {252, 258},
		{260, 261}, {263, 265},
		{267, 272}, {274, 275}, 
		467, {469, 471}, {1203, 1204},
		{1209, 1210}, {1221, 1222}, 1226 
	}), Block, 30, 30)
	objs((
		{ 64, 195, 206, 220, 661,
		{1155, 1157}, 1208, 1910
	}), Block, 15, 15)
	objs(({ 40, 147, 215, {369, 370}, {1903, 1905} }), Block, 30, 14)
	objs(({ { 170, 172 }, 174, 192 }), Block, 30, 21)
	objs(({ 468, 475, 1260 }), Block, 30, 1.5)
	objs(({ 62, 65, 66, 68 }), Block, 30, 16)
	objs(({ 1202, 1262 }), Block, 30, 3)
	objs(({ 1220, 1264 }), Block, 30, 6)
	objs(({ 196, 219, 1911 }), Block, 15, 8)
	objs(({ 204 }), Block, 8, 15)
	objs(({ 662, 663, 664 }), Block, 30, 15)
	objs(({ 1561 }), Block, 30, 10)
	objs(({ 1567 }), Block, 15, 10)
	objs(({ 1566 }), Block, 12, 12)
	objs(({ 1565 }), Block, 17, 17)
	objs(({ 1227 }), Block, 30, 7)
	objs(({ 328 }), Block, 22, 22)
	objs(({ 197 }), Block, 22, 21)
	objs(({ 194 }), Block, 21, 21)
	objs(({ 176 }), Block, 14, 21)
	objs(({ 1562 }), Block, 30, 2)
	objs(({ 1343 }), Block, 25, 3)
	objs(({ 1340 }), Block, 27, 2)
	objs(({ 34 }), Block, 37, 23)

	objs(({ 720, 991, 1731, 1733 }), Hazard, 2.40039063, 3.20001221)
	objs(({ 61, 446, 1719, 1728 }), Hazard, 9, 7.2)
	objs(({ 365, 667, 1716, 1730 }), Hazard, 9, 6)
	objs(({ 392, {458, 459} }), Hazard, 2.6, 4.8)
	objs(({ 8, 144, 177, 216 }), Hazard, 6, 12)
	objs(({ 103, 145, 218 }), Hazard, 4, 7.6)
	objs(({ 39, 205, 217 }), Hazard, 6, 5.6)
	objs(({ 768, 1727 }), Hazard, 4.5, 5.2)
	objs(({ 447, 1729 }), Hazard, 5.2, 7.2)
	objs(({ 135, 1711 }), Hazard, 14.1, 20)
	objs(({ 422, 1726 }), Hazard, 6, 4.4)
	objs(({ 244, 1721 }), Hazard, 6, 6.8)
	objs(({ 243, 1720 }), Hazard, 6, 7.2)
	objs(({ 421, 1725 }), Hazard, 9, 5.2)
	objs(({ 9, 1715 }), Hazard, 9, 10.8)
	objs(({ 989, 1732 }), Hazard, 9, 12)
	objs(({ 1714 }), Hazard, 11.4, 16.4)
	objs(({ 1712 }), Hazard, 13.5, 22.4)
	objs(({ 368, 1722 }), Hazard, 9, 4)
	objs(({ 1713 }), Hazard, 11.7, 20)
	objs(({ 178 }), Hazard, 6, 6.4)
	objs(({ 919 }), Hazard, 25, 6)
	objs(({ 179 }), Hazard, 4, 8)

	objs(({ 88, 186, 740, 1705 }), Sawblade, 32.3, 32.3)
	objs(({ 89, 1706 }), Sawblade, 21.6, 21.6)
	objs(({ 98 }), Sawblade, 12, 12)
	objs(({ 183 }), Sawblade, 15.660001, 15.660001)
	objs(({ 184 }), Sawblade, 20.4, 20.4)
	objs(({ 185 }), Sawblade, 2.8500001, 2.8500001)
	objs(({ 187, 741 }), Sawblade, 21.960001, 21.960001)
	objs(({ 188, 742 }), Sawblade, 12.6000004, 12.6000004)
	objs(({ 397, 1708 }), Sawblade, 28.9, 28.9)
	objs(({ 398, 1709 }), Sawblade, 17.44, 17.44)
	objs(({ 399, 1710 }), Sawblade, 12.900001, 12.900001)
	objs(({ 675, 1734 }), Sawblade, 32, 32)
	objs(({ 676, 1735 }), Sawblade, 17.5100002, 17.5100002)
	objs(({ 677, 1736 }), Sawblade, 12.479999, 12.479999)
	objs(({ 678 }), Sawblade, 30.4, 30.4)
	objs(({ 679 }), Sawblade, 18.54, 18.54)
	objs(({ 680 }), Sawblade, 10.8, 10.8)
	objs(({ 918 }), Sawblade, 24, 24)
	objs(({ 1582, 1583 }), Sawblade, 4, 4)
	objs(({ 1619 }), Sawblade, 25, 25)
	objs(({ 1620 }), Sawblade, 15, 15)
	objs(({ 1707 }), Sawblade, 12, 12)
	objs(({ {1701, 1703} }), Sawblade, 6, 6)

	objs(({ 35 }), Pad, 25, 4)
	objs(({ 140 }), Pad, 25, 5)
	objs(({ 67 }), Pad, 25, 6)
	objs(({ 1332 }), Pad, 25, 6)
	objs(({ 1402 }), Pad, 25, 6)
	objs(({ 36, 84, 141, 1022, 1330, 1333, 1410, 1411, 1418, 1704, 1751 }), Orb, 36, 36)

	objs(({ 12, 13, 47, 111, 660, 745, 1331, 1933 }), VehiclePortal, 34, 86)
	objs(({ 10, 11 }), GravityPortal, 25, 75)

	objs(({ 99, 101 }), SizePortal, 31, 90)

	objs(({ 45, 46 }), MirrorPortal, 30, 80)
	objs(({ 286, 287 }), DualPortal, 30, 80)

	objs(({ 1813, 1814, 1815, 1816 }), SpecialBlock, 30, 30)

	objs(({ 110, 143 }), BreakableBlock, 30, 30)

	objs(({ 200 }), SpeedPortal, 35, 44)
	objs(({ 201 }), SpeedPortal, 33, 56)
	objs(({ 202 }), SpeedPortal, 51, 56)
	objs(({ 203 }), SpeedPortal, 65, 56)
	objs(({ 1334 }), SpeedPortal, 69, 56)

	// Teleport portals (yellow, blue, green, pink)
	objs(({ 747, 748, 749, 750 }), TeleportPortal, 30, 30)

	objs(({
		289, 294, 299, 305, 309, 315, 321, 326, 331, 337,
		343, 349, 353, 371, 483, 492, 651, 665,
		673, 709, 711, 726, 728, 886, 1338, 1341, 1344,
		1723, 1743, 1745, 1747, 1749, 1906
	}), Slope, 30, 30)
	objs(({ 363, 1717 }), SlopeHazard, 30, 30);

	objs(({
		291, 295, 301, 307, 311, 317, 323, 327, 333, 339,
		345, 351, 355, 367, 372, 484, 493, 652, 666,
		674, 710, 712, 727, 729, 887, 1339, 1342, 1345,
		1724, 1744, 1746, 1748, 1750, 1907
	}), Slope, 60, 30)
	objs(({ 364, 366, 1718 }), SlopeHazard, 60, 30);

	// Extended 2.2 block coverage: base tiles of the block001/002/005/005b/
	// 005c/006/007/007b/008/009/009b/009c/010/011/011b/012/012b/013 families
	// (numbered _NN tiles, b/c color variants and _base cores share the same
	// full-grid sprite), plus planks, puzzle bases, bricks and glow squares.
	// Sourced from the texture->id map (2.2074 object sprites): every id
	// below was verified to exist and to use a full-grid base sprite, so the
	// standard 30x30 solid hitbox applies ("hitboxes are identical to how
	// they appear visually"). Deliberately NOT mapped: trim/detail overlays
	// (*_edge, *_light, *_detail, *_topcolor, *_bgcolor, *_piece, *_part) are
	// sub-tile overlays — full hitboxes there would create phantom walls;
	// perspective outlines, deco (d_*), editor icons and fake spikes are
	// non-solid by design.
	objs(({
		{476, 482}, {485, 491}, {752, 759}, {903, 905}, 911,
		{952, 959}, {974, 977}, {1299, 1304}, {1310, 1315},
		{1162, 1186}, {1191, 1197}, 1322, {807, 825}, {1082, 1090},
		{841, 848}, 850, {853, 857}, 859, {861, 863},
		{867, 872}, {1266, 1267}, {880, 885}, 1062, {1159, 1160},
		{890, 891}, {893, 894}, {1247, 1255}, {1277, 1290},
		{927, 930}, {932, 934}, {1239, 1240}, {1348, 1355}, {1777, 1784},
		{1431, 1452}, {1510, 1515}, 1793, 1795, 1799, 1801, 1803, 1805, 1807, 1809,
		{1461, 1464}, {1769, 1772}, {373, 374}, {769, 770}, {967, 968},
		{1079, 1081}, {1096, 1098}, {1908, 1909}, {1685, 1696}, 1797,
		{245, 246}, {1893, 1898}, 211, 251, 259, 266, 273, {277, 278},
		{1820, 1821}, {1823, 1828}
	}), Block, 30, 30)
	// Small 2.2 tiles: block007_01_small shares the half-size class.
	objs(({ 1078, 1095 }), Block, 15, 15)
	// Triangle-capped squares: same family as the calibrated 328 (22x22).
	objs(({ {296, 297}, {324, 325}, 329, 358, {1901, 1902} }), Block, 22, 22)
	// Outline-thick slabs: same thickness class as the calibrated
	// Thick_01 (30x3) and Thickb_01 (30x6) entries above.
	objs(({ {1205, 1207} }), Block, 30, 30)
	objs(({ {1223, 1225} }), Block, 30, 30)

	// Extended 2.2 slope pieces (*_slope, *_slope_square) and small triangle
	// slopes. The _02 suffix behaves like the calibrated triangle_h_01
	// (30x30 slope) and _04 like triangle_h_02 (60x30 slope).
	objs(({
		681, 683, 685, 689, 691, 687, 693, 695, 697, 699, 701,
		{703, 706}, {762, 766}, {1033, 1036}, {960, 961}, {1037, 1038},
		{964, 966}, {1039, 1040}, {1305, 1309}, {1316, 1320},
		{1187, 1190}, {1198, 1201}, {1325, 1326}, {826, 829}, {1091, 1092}, 1108,
		{830, 833}, {1093, 1094}, {877, 878}, {888, 889}, {895, 896}, {1256, 1259},
		{1014, 1018}, {771, 775}, {969, 973}
	}), Slope, 30, 30)
	objs(({
		682, 684, 686, 690, 692, 688, 694, 696, 698, 700, 702
	}), Slope, 60, 30)

	// Static fire and lava surfaces kill on contact. Hitboxes are estimates:
	// flames are narrower/shorter than the full tile, lava kills at the
	// surface strip.
	objs(({ {920, 921}, {923, 924}, {1936, 1939} }), Hazard, 20, 16)
	objs(({ 1591, 1593 }), Hazard, 30, 8)

	// Any block that isnt' defined is ignored
	return {};
}


Object::Object(Vec2D s, std::unordered_map<int, std::string>&& fields) {
	size = s;

	pos.x = stod_def(fields[2]);
	pos.y = stod_def(fields[3]);
	rotation = -stod_def(fields[6]);
	prio = 0;

	float scale = stod_def(fields[32], 1);
	float scalex = stod_def(fields[128], 1);
	float scaley = stod_def(fields[129], 1);

	size.x *= scale * scalex;
	size.y *= scale * scaley;
}

bool Object::touching(Player const& player) const {
	int r = std::abs(rotation);
	return intersects((r == 0 || r == 90 || r == 180 || r == 270) ? player.unrotatedHitbox() : static_cast<Entity const&>(player));
}

void Object::collide(Player&) const {
	// Unreachable
	abort();
}
