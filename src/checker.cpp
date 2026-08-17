#include "checker.hpp"
#include "input_scheduler.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <gdr/gdr.hpp>
#include <algorithm>
#include <deque>
#include <limits>
#include <random>

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

std::vector<uint8_t> pathfindInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(double)> const& progress
) {
    if (!level)
        return {};

    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer)
        return {};
    playLayer->retain();
    playLayer->setVisible(false);
    playLayer->resetLevel();

    CaptureGuard capture;
    constexpr float physicsStep = 1.f / 240.f;
    constexpr uint32_t horizon = 240;
    constexpr uint32_t commitFrames = 120;
    constexpr int trialsPerChunk = 64;
    constexpr int maxChunks = 2400;

    struct Event {
        uint32_t frame;
        bool player2;
        bool down;
    };
    struct SavedPoint {
        CheckpointObject* checkpoint;
        uint32_t frame;
        bool p1Down;
        bool p2Down;
        size_t inputCount;
    };
    struct Trial {
        std::vector<Event> events;
        uint32_t survived = 0;
        float score = -std::numeric_limits<float>::infinity();
        bool completed = false;
    };

    std::vector<SavedPoint> history;
    std::vector<Event> committed;
    auto savePoint = [&](uint32_t frame, bool p1Down, bool p2Down) {
        auto checkpoint = playLayer->createCheckpoint();
        checkpoint->retain();
        history.push_back({checkpoint, frame, p1Down, p2Down, committed.size()});
    };
    savePoint(0, false, false);

    auto restore = [&](SavedPoint const& point, bool& liveP1, bool& liveP2) {
        // Input state is not part of PlayerCheckpoint, so normalize it before
        // loading the complete game/object/trigger snapshot.
        playLayer->handleButton(false, 1, false);
        playLayer->handleButton(false, 1, true);
        playLayer->loadFromCheckpoint(point.checkpoint);
        if (point.p1Down)
            playLayer->handleButton(true, 1, false);
        if (point.p2Down)
            playLayer->handleButton(true, 1, true);
        liveP1 = point.p1Down;
        liveP2 = point.p2Down;
        s_collision.died = false;
        s_collision.objectID = 0;
        s_collision.position = CCPointZero;
    };

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> chance(0., 1.);
    bool solved = false;

    for (int chunk = 0; chunk < maxChunks && !stop && !solved; ++chunk) {
        auto const base = history.back();
        auto ticks = pathfinder::fixedTickFrames(
            base.frame + 1, base.frame + horizon, physicsStep
        );
        Trial best;

        for (int trialIndex = 0; trialIndex < trialsPerChunk && !stop; ++trialIndex) {
            Trial trial;
            bool plannedP1 = base.p1Down;
            bool plannedP2 = base.p2Down;

            // Trial zero is the no-input continuation. Trial one is a dense
            // pattern useful for robot/cube/ball holds. Other trials are sparse;
            // spider and swing therefore treat 70 as a cap, never forced spam.
            for (auto frame : ticks) {
                const double p = trialIndex == 1 ? .5 : .10;
                if (trialIndex != 0 && chance(rng) < p) {
                    plannedP1 = !plannedP1;
                    trial.events.push_back({frame, false, plannedP1});
                }
                if (trialIndex != 0 && chance(rng) < p) {
                    plannedP2 = !plannedP2;
                    trial.events.push_back({frame, true, plannedP2});
                }
            }
            std::stable_sort(trial.events.begin(), trial.events.end(), [](auto const& a, auto const& b) {
                return a.frame < b.frame;
            });

            bool liveP1 = false;
            bool liveP2 = false;
            restore(base, liveP1, liveP2);
            size_t eventIndex = 0;
            for (uint32_t offset = 1; offset <= horizon; ++offset) {
                const uint32_t frame = base.frame + offset;
                while (eventIndex < trial.events.size() && trial.events[eventIndex].frame == frame) {
                    auto const event = trial.events[eventIndex++];
                    playLayer->handleButton(event.down, 1, event.player2);
                    (event.player2 ? liveP2 : liveP1) = event.down;
                }
                playLayer->update(physicsStep);
                trial.survived = offset;
                if (playLayer->m_hasCompletedLevel) {
                    trial.completed = true;
                    break;
                }
                if (s_collision.died)
                    break;
            }

            const float p1x = playLayer->m_player1->getPositionX();
            const float p2x = playLayer->m_isDualMode
                ? playLayer->m_player2->getPositionX() : p1x;
            // Survival dominates position. Position breaks ties in platformer,
            // reverse and teleport sections where frame count alone is weak.
            trial.score = static_cast<float>(trial.survived) * 100000.f + std::min(p1x, p2x);
            if (trial.completed || trial.score > best.score)
                best = std::move(trial);
            if (best.completed)
                break;
        }

        if (stop)
            break;

        if (best.completed) {
            committed.insert(committed.end(), best.events.begin(), best.events.end());
            solved = true;
            break;
        }

        // Never commit the collision frame. If every branch dies immediately,
        // backtrack one exact game checkpoint and explore a different branch.
        const uint32_t safeFrames = best.survived > 2 ? best.survived - 2 : 0;
        const uint32_t advance = std::min(commitFrames, safeFrames);
        if (advance < 8) {
            if (history.size() > 1) {
                history.back().checkpoint->release();
                history.pop_back();
                committed.resize(history.back().inputCount);
            }
            continue;
        }

        bool liveP1 = false;
        bool liveP2 = false;
        restore(base, liveP1, liveP2);
        size_t eventIndex = 0;
        for (uint32_t offset = 1; offset <= advance; ++offset) {
            const uint32_t frame = base.frame + offset;
            while (eventIndex < best.events.size() && best.events[eventIndex].frame == frame) {
                auto const event = best.events[eventIndex++];
                playLayer->handleButton(event.down, 1, event.player2);
                (event.player2 ? liveP2 : liveP1) = event.down;
                committed.push_back(event);
            }
            playLayer->update(physicsStep);
            if (s_collision.died)
                break;
        }
        if (s_collision.died)
            continue;

        savePoint(base.frame + advance, liveP1, liveP2);
        if (progress)
            progress(std::clamp<double>(playLayer->getCurrentPercent(), 0., 100.));
    }

    PathfinderReplay output;
    if (solved) {
        std::stable_sort(committed.begin(), committed.end(), [](auto const& a, auto const& b) {
            return a.frame < b.frame;
        });
        for (auto const& event : committed)
            output.inputs.emplace_back(event.frame, 1, event.player2, event.down);
    }

    for (auto& point : history)
        point.checkpoint->release();
    playLayer->release();
    if (!solved)
        return {};
    return output.exportData().unwrapOr({});
}
