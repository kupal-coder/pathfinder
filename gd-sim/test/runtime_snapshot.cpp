#include <Level.hpp>
#include <cassert>
#include <cmath>

int main() {
    RuntimeLevelSnapshot snapshot;
    snapshot.rawLevelString =
        "kA2,0,kA3,0,kA4,0,kA11,0;";
    snapshot.levelLength = 900;

    RuntimeObjectSnapshot solid;
    solid.uniqueID = 42;
    solid.editorObjectID = 999999; // intentionally unknown to Object.cpp tables
    solid.kind = RuntimeObjectKind::Solid;
    solid.position = {120, 75};
    solid.hitboxSize = {47, 13};
    solid.rotation = 27;
    snapshot.objects.push_back(solid);

    Level level(snapshot);
    assert(level.length == 900);
    assert(level.objectCount == 1);
    assert(level.sections.size() > 1);
    auto const& object = level.sections[1].front();
    assert(object->id == 42);
    assert(object->pos == Vec2D(120, 75));
    assert(object->size == Vec2D(47, 13));
    assert(std::abs(object->rotation - 27) < .001f);
}
