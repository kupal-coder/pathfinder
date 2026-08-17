#include "checker.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <gdr/gdr.hpp>
#include <algorithm>
#include <deque>

using namespace geode::prelude;

namespace {

class PathfinderReplay : public gdr::Replay<PathfinderReplay, gdr::Input<"">> {
public:
    PathfinderReplay() : Replay("Pathfinder", 1) {}
};

struct CollisionCapture {
    bool active = false;
    bool died = false;
    int objectID = 0;
    CCPoint position = CCPointZero;
};

CollisionCapture s_collision;

class $modify(PathfinderCollisionCapture, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (s_collision.active) {
            s_collision.died = true;
            if (object) {
                s_collision.objectID = object->m_objectID;
                s_collision.position = object->getPosition();
            }
        }
        PlayLayer::destroyPlayer(player, object);
    }
};

struct CaptureGuard {
    CaptureGuard() {
        s_collision = {};
        s_collision.active = true;
    }
    ~CaptureGuard() { s_collision.active = false; }
};

} // namespace

VerificationResult verifyInGame(GJGameLevel* level, std::vector<uint8_t> const& macro) {
    VerificationResult result;
    if (!level) {
        result.error = "missing level";
        return result;
    }
    if (macro.empty()) {
        result.error = "pathfinder returned an empty replay";
        return result;
    }

    auto imported = PathfinderReplay::importData(macro);
    if (imported.isErr()) {
        result.error = "could not decode generated replay";
        return result;
    }

    auto inputs = imported.unwrap().inputs;
    std::stable_sort(inputs.begin(), inputs.end(), [](auto const& lhs, auto const& rhs) {
        return lhs.frame < rhs.frame;
    });
    std::deque<gdr::Input<"">> pending(inputs.begin(), inputs.end());

    // PlayLayer::create constructs the exact runtime object collection. Its
    // normal update runs spawn/move/toggle triggers and all 2.2 collision code.
    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer) {
        result.error = "Geometry Dash could not create a verification PlayLayer";
        return result;
    }
    playLayer->retain();
    playLayer->setVisible(false);
    playLayer->resetLevel();

    CaptureGuard capture;
    constexpr float step = 1.f / 240.f;
    // A malformed level must not freeze the UI forever. 20 minutes is above
    // the editor's practical level duration while still providing a hard stop.
    constexpr int maxFrames = 20 * 60 * 240;

    for (int frame = 1; frame <= maxFrames; ++frame) {
        result.frame = frame;
        while (!pending.empty() && pending.front().frame <= frame) {
            auto const input = pending.front();
            playLayer->handleButton(input.down, input.button, input.player2);
            pending.pop_front();
        }

        // This is deliberately the game's update, not a parallel physics step.
        playLayer->update(step);
        if (s_collision.died)
            break;
        if (playLayer->m_hasCompletedLevel) {
            result.completed = true;
            break;
        }
    }

    result.died = s_collision.died;
    result.objectID = s_collision.objectID;
    result.objectX = s_collision.position.x;
    result.objectY = s_collision.position.y;
    if (!result.completed && !result.died && result.error.empty())
        result.error = "verification timed out before level completion";

    playLayer->release();
    return result;
}
