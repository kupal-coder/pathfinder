#include "checker.hpp"
#include "input_scheduler.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <gdr/gdr.hpp>
#include <algorithm>
#include <bit>
#include <deque>
#include <limits>
#include <random>
#include <unordered_set>

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
    float rotation = 0;
    float scaleX = 1;
    float scaleY = 1;
    bool player2 = false;
    CCPoint playerPosition = CCPointZero;
    double playerYVelocity = 0;
};

CollisionCapture s_collision;

class $modify(PathfinderCollisionCapture, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (s_collision.active) {
            s_collision.died = true;
            if (player) {
                s_collision.player2 = player->isPlayer2();
                s_collision.playerPosition = player->getPosition();
                s_collision.playerYVelocity = player->m_yVelocity;
            }
            if (object) {
                s_collision.objectID = object->m_objectID;
                s_collision.position = object->getPosition();
                s_collision.rotation = object->getRotation();
                s_collision.scaleX = object->getScaleX();
                s_collision.scaleY = object->getScaleY();
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

void setRuntimeSeed(PlayLayer* layer, uint32_t seed) {
    layer->m_randomSeed = seed;
    layer->m_replayRandSeed = seed;
    GameToolbox::fast_srand(seed);
}

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

    auto replay = imported.unwrap();
    auto inputs = replay.inputs;
    const auto runtimeSeed = static_cast<uint32_t>(replay.seed);
    std::stable_sort(inputs.begin(), inputs.end(), [](auto const& lhs, auto const& rhs) {
        return lhs.frame < rhs.frame;
    });
    pathfinder::ClickRateLimiter clickLimiter;
    for (auto const& input : inputs) {
        if (input.down && input.button == 1 &&
            !clickLimiter.accept(input.frame, input.player2)) {
            result.frame = static_cast<int>(input.frame);
            result.player2 = input.player2;
            result.error = "replay exceeds the 70 CPS cap";
            return result;
        }
    }
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
    setRuntimeSeed(playLayer, runtimeSeed);
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

        auto mix = [&](uint64_t value) {
            result.traceHash ^= value;
            result.traceHash *= 1099511628211ull;
        };
        auto mixPlayer = [&](PlayerObject* player) {
            mix(std::bit_cast<uint32_t>(player->getPositionX()));
            mix(std::bit_cast<uint32_t>(player->getPositionY()));
            mix(std::bit_cast<uint64_t>(player->m_yVelocity));
            uint64_t flags = player->m_isShip | (player->m_isBall << 1) |
                (player->m_isBird << 2) | (player->m_isDart << 3) |
                (player->m_isRobot << 4) | (player->m_isSpider << 5) |
                (player->m_isSwing << 6) | (player->m_isUpsideDown << 7);
            mix(flags);
        };
        mixPlayer(playLayer->m_player1);
        if (playLayer->m_isDualMode)
            mixPlayer(playLayer->m_player2);
        mix(playLayer->m_isDualMode);
        mix(playLayer->m_randomSeed);

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
    result.objectRotation = s_collision.rotation;
    result.objectScaleX = s_collision.scaleX;
    result.objectScaleY = s_collision.scaleY;
    result.player2 = s_collision.player2;
    result.playerX = s_collision.playerPosition.x;
    result.playerY = s_collision.playerPosition.y;
    result.playerYVelocity = s_collision.playerYVelocity;
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
    const auto configuredSeed = Mod::get()->getSettingValue<int64_t>("search-seed");
    const uint32_t runtimeSeed = configuredSeed == 0
        ? (std::random_device{}() & 0x7fffffffu)
        : static_cast<uint32_t>(configuredSeed);
    log::info("Pathfinder runtime seed: {}", runtimeSeed);
    setRuntimeSeed(playLayer, runtimeSeed);
    playLayer->resetLevel();

    CaptureGuard capture;
    constexpr float physicsStep = 1.f / 240.f;
    uint32_t horizon = 120;
    constexpr uint32_t minHorizon = 60;
    constexpr uint32_t maxHorizon = 240;
    constexpr uint32_t commitFrames = 120;
    constexpr int trialsPerChunk = 64;
    constexpr int maxChunks = 2400;

    struct Event {
        uint32_t frame;
        uint8_t button;
        bool player2;
        bool down;
    };
    struct Buttons {
        bool jump = false;
        bool left = false;
        bool right = false;
    };
    struct SavedPoint {
        CheckpointObject* checkpoint;
        uint32_t frame;
        Buttons p1;
        Buttons p2;
        size_t inputCount;
    };
    struct Trial {
        std::vector<Event> events;
        uint32_t survived = 0;
        float score = -std::numeric_limits<float>::infinity();
        uint64_t stateHash = 0;
        bool completed = false;
    };

    std::vector<SavedPoint> history;
    std::vector<Event> committed;
    auto savePoint = [&](uint32_t frame, Buttons p1, Buttons p2) {
        auto checkpoint = playLayer->createCheckpoint();
        checkpoint->retain();
        history.push_back({checkpoint, frame, p1, p2, committed.size()});
        constexpr size_t maxRetainedCheckpoints = 256;
        if (history.size() > maxRetainedCheckpoints) {
            history.front().checkpoint->release();
            history.erase(history.begin());
        }
    };
    savePoint(0, {}, {});

    auto restore = [&](SavedPoint const& point, Buttons& liveP1, Buttons& liveP2) {
        // Input state is not part of PlayerCheckpoint, so normalize every
        // gameplay button before loading the object/trigger snapshot.
        for (uint8_t button = 1; button <= 3; ++button) {
            playLayer->handleButton(false, button, false);
            playLayer->handleButton(false, button, true);
        }
        playLayer->loadFromCheckpoint(point.checkpoint);
        auto apply = [&](Buttons const& buttons, bool player2) {
            if (buttons.jump) playLayer->handleButton(true, 1, player2);
            if (buttons.left) playLayer->handleButton(true, 2, player2);
            if (buttons.right) playLayer->handleButton(true, 3, player2);
        };
        apply(point.p1, false);
        apply(point.p2, true);
        liveP1 = point.p1;
        liveP2 = point.p2;
        s_collision.died = false;
        s_collision.objectID = 0;
        s_collision.position = CCPointZero;
    };

    std::mt19937 rng(runtimeSeed ^ 0x9e3779b9u);
    std::uniform_real_distribution<double> chance(0., 1.);
    auto runtimeStateHash = [&] {
        uint64_t hash = 1469598103934665603ull;
        auto mix = [&](uint64_t value) {
            hash ^= value;
            hash *= 1099511628211ull;
        };
        auto player = [&](PlayerObject* value) {
            mix(std::bit_cast<uint32_t>(value->getPositionX()));
            mix(std::bit_cast<uint32_t>(value->getPositionY()));
            mix(std::bit_cast<uint64_t>(value->m_yVelocity));
            mix(std::bit_cast<uint64_t>(value->m_platformerXVelocity));
            mix(value->m_isShip | (value->m_isBall << 1) |
                (value->m_isBird << 2) | (value->m_isDart << 3) |
                (value->m_isRobot << 4) | (value->m_isSpider << 5) |
                (value->m_isSwing << 6) | (value->m_isUpsideDown << 7));
        };
        player(playLayer->m_player1);
        if (playLayer->m_isDualMode)
            player(playLayer->m_player2);
        mix(playLayer->m_currentStep);
        mix(playLayer->m_commandIndex);
        mix(playLayer->m_randomSeed);
        for (auto* object : playLayer->m_activeObjects) {
            if (!object)
                continue;
            mix(static_cast<uint32_t>(object->m_uniqueID));
            mix(std::bit_cast<uint32_t>(object->getPositionX()));
            mix(std::bit_cast<uint32_t>(object->getPositionY()));
            mix(std::bit_cast<uint32_t>(object->getRotation()));
            mix(object->m_isDisabled | (object->m_isActivated << 1));
        }
        return hash;
    };
    bool solved = false;

    for (int chunk = 0; chunk < maxChunks && !stop && !solved; ++chunk) {
        auto const base = history.back();
        auto ticks = pathfinder::fixedTickFrames(
            base.frame + 1, base.frame + horizon, physicsStep
        );
        Trial best;
        std::unordered_set<uint64_t> seenStates;

        for (int trialIndex = 0; trialIndex < trialsPerChunk && !stop; ++trialIndex) {
            Trial trial;
            struct Decision {
                uint32_t frame;
                bool p1;
                bool p2;
                int8_t p1Direction;
                int8_t p2Direction;
            };
            std::vector<Decision> decisions;
            decisions.reserve(ticks.size());
            for (auto frame : ticks) {
                decisions.push_back({
                    frame,
                    trialIndex != 0 && chance(rng) < .10,
                    trialIndex != 0 && chance(rng) < .10,
                    trialIndex != 0 && chance(rng) < .06 ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1),
                    trialIndex != 0 && chance(rng) < .06 ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1)
                });
            }

            Buttons liveP1;
            Buttons liveP2;
            restore(base, liveP1, liveP2);
            size_t decisionIndex = 0;
            uint32_t releaseP1 = 0;
            uint32_t releaseP2 = 0;

            auto send = [&](uint32_t frame, uint8_t button, bool player2, bool down, bool& live) {
                if (live == down)
                    return;
                playLayer->handleButton(down, button, player2);
                live = down;
                trial.events.push_back({frame, button, player2, down});
            };

            auto applyDecision = [&](
                uint32_t frame,
                bool player2,
                bool selected,
                PlayerObject* player,
                Buttons& live,
                uint32_t& releaseFrame
            ) {
                const bool actionMode = player->m_isSpider || player->m_isSwing;
                const bool harmlessSpam = player->m_isRobot || player->m_isBall ||
                    (!player->m_isShip && !player->m_isBird && !player->m_isDart &&
                     !player->m_isSpider && !player->m_isSwing);

                // The dense candidate exists only in harmless modes. Spider
                // and swing continue using random selected actions, even when
                // this is trial one, because every press changes their state.
                const bool pulse = actionMode ? selected
                    : (trialIndex == 1 && harmlessSpam);
                if (pulse) {
                    // A pulse is one press on a 70 Hz boundary followed by a
                    // release on the next physics frame. If a portal changed
                    // mode while held, normalize the old hold first.
                    send(frame, 1, player2, false, live.jump);
                    send(frame, 1, player2, true, live.jump);
                    releaseFrame = frame + 1;
                } else if (selected) {
                    // Ship/UFO/wave and ordinary non-spam candidates need real
                    // holds, so decisions toggle rather than pulse.
                    send(frame, 1, player2, !live.jump, live.jump);
                }
            };

            for (uint32_t offset = 1; offset <= horizon; ++offset) {
                const uint32_t frame = base.frame + offset;
                if (releaseP1 == frame) {
                    send(frame, 1, false, false, liveP1.jump);
                    releaseP1 = 0;
                }
                if (releaseP2 == frame) {
                    send(frame, 1, true, false, liveP2.jump);
                    releaseP2 = 0;
                }

                if (decisionIndex < decisions.size() && decisions[decisionIndex].frame == frame) {
                    auto const decision = decisions[decisionIndex++];
                    applyDecision(frame, false, decision.p1, playLayer->m_player1, liveP1, releaseP1);
                    applyDecision(frame, true, decision.p2, playLayer->m_player2, liveP2, releaseP2);

                    auto applyDirection = [&](bool player2, int8_t direction, PlayerObject* player, Buttons& live) {
                        if (!player->m_isPlatformer || direction < 0)
                            return;
                        const bool wantLeft = direction == 1;
                        const bool wantRight = direction == 2;
                        send(frame, 2, player2, wantLeft, live.left);
                        send(frame, 3, player2, wantRight, live.right);
                    };
                    applyDirection(false, decision.p1Direction, playLayer->m_player1, liveP1);
                    applyDirection(true, decision.p2Direction, playLayer->m_player2, liveP2);
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

            trial.stateHash = runtimeStateHash();
            if (!trial.completed && !seenStates.insert(trial.stateHash).second)
                continue;

            // Survival dominates, while the game's own progress calculation
            // breaks ties. Unlike raw X this also works after reverse portals,
            // teleports, dual splits, and in platformer levels.
            trial.score = static_cast<float>(trial.survived) * 100000.f +
                playLayer->getCurrentPercent() * 1000.f;
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
            horizon = std::max(minHorizon, horizon / 2);
            if (history.size() > 1) {
                history.back().checkpoint->release();
                history.pop_back();
                committed.resize(history.back().inputCount);
            }
            continue;
        }

        Buttons liveP1;
        Buttons liveP2;
        restore(base, liveP1, liveP2);
        size_t eventIndex = 0;
        for (uint32_t offset = 1; offset <= advance; ++offset) {
            const uint32_t frame = base.frame + offset;
            while (eventIndex < best.events.size() && best.events[eventIndex].frame == frame) {
                auto const event = best.events[eventIndex++];
                playLayer->handleButton(event.down, event.button, event.player2);
                auto& buttons = event.player2 ? liveP2 : liveP1;
                auto& state = event.button == 1 ? buttons.jump :
                    (event.button == 2 ? buttons.left : buttons.right);
                state = event.down;
                committed.push_back(event);
            }
            playLayer->update(physicsStep);
            if (s_collision.died)
                break;
        }
        if (s_collision.died)
            continue;

        savePoint(base.frame + advance, liveP1, liveP2);
        if (best.survived >= horizon)
            horizon = std::min(maxHorizon, horizon + 30);
        else if (best.survived < horizon / 2)
            horizon = std::max(minHorizon, horizon / 2);
        if (progress)
            progress(std::clamp<double>(playLayer->getCurrentPercent(), 0., 100.));
    }

    PathfinderReplay output;
    output.seed = static_cast<int>(runtimeSeed);
    if (solved) {
        std::stable_sort(committed.begin(), committed.end(), [](auto const& a, auto const& b) {
            return a.frame < b.frame;
        });
        for (auto const& event : committed)
            output.inputs.emplace_back(event.frame, event.button, event.player2, event.down);
    }

    for (auto& point : history)
        point.checkpoint->release();
    playLayer->release();
    if (!solved)
        return {};
    return output.exportData().unwrapOr({});
}
