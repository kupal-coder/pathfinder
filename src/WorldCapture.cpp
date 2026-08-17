#include "WorldCapture.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

/**
 * Build the search world from the game's live object list.
 *
 * The previous approach re-parsed the level string and looked every object id
 * up in a hardcoded table. That table covered 314 ids topping out at 1911 and
 * contained no 2.2 objects at all, and anything missing was silently dropped --
 * so the search planned routes through geometry that exists in game. That is
 * the phasing bug.
 *
 * Reading `m_objects` instead means:
 *
 *  - every object is present, whatever its id, including everything from 2.2;
 *  - geometry comes from the object's own rect and oriented box, so scale and
 *    rotation are exactly what the game will collide against;
 *  - objects the game has already moved or toggled are captured where they
 *    actually are, not where the level string first placed them.
 */
std::vector<RuntimeObject> captureWorld(GJBaseGameLayer* layer, WorldCaptureStats* stats) {
	std::vector<RuntimeObject> out;

	if (!layer || !layer->m_objects)
		return out;

	auto* objects = layer->m_objects;
	out.reserve(objects->count());

	for (unsigned i = 0; i < objects->count(); ++i) {
		auto* obj = static_cast<GameObject*>(objects->objectAtIndex(i));
		if (!obj)
			continue;

		RuntimeObject ro;
		ro.objectId = obj->m_objectID;
		ro.rawType = static_cast<int>(obj->m_objectType);
		ro.behaviour = behaviourFromGameType(ro.rawType);
		ro.isTrigger = obj->m_isTrigger;

		// A trigger is never solid, whatever its type tag says.
		if (ro.isTrigger)
			ro.behaviour = ObjectBehaviour::Trigger;

		auto pos = obj->getPosition();
		ro.x = pos.x;
		ro.y = pos.y;
		ro.rotation = obj->getRotation();
		ro.radius = obj->m_objectRadius;

		// Geometry straight from the game. getObjectRect() already accounts for
		// scale and the object's own hitbox definition, so there is no table to
		// keep in sync and no chance of disagreeing with the collision the game
		// will actually perform.
		auto rect = obj->getObjectRect();
		ro.width = rect.size.width;
		ro.height = rect.size.height;

		// Objects that start hidden or disabled do not collide until a trigger
		// enables them. Recorded so the trigger pass can switch them on.
		ro.startsDisabled = obj->m_isDisabled || obj->m_isHide;

		if (stats) {
			++stats->total;
			if (!behaviourSupported(ro.behaviour))
				stats->unsupported.push_back(ro.objectId);
			if (ro.behaviour == ObjectBehaviour::UnsupportedSolid)
				++stats->unclassified;
		}

		out.push_back(std::move(ro));
	}

	return out;
}
