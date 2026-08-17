#include "checker.hpp"
#include "input_scheduler.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <gdr/gdr.hpp>
#include <algorithm>
#include <bit>
#include <deque>
#include <limits>
#include <memory>
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

RuntimeSearchTask pathfindInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(double)> progress
) {
    if (!level)
        co_return std::vector<uint8_t>{};

    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer)
        co_return std::vector<uint8_t>{};
    playLayer->retain();
    struct LayerLifetime {
        PlayLayer* layer;
        ~LayerLifetime() { layer->release(); }
    } layerLifetime {playLayer};
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
    constexpr uint32_t minHorizon = 60;
    constexpr uint32_t maxHorizon = 240;
    constexpr size_t beamWidth = 6;
    constexpr int branchesPerNode = 12;
    constexpr int maxGenerations = 4800;
    uint32_t horizon = 120;

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
    using CheckpointPtr = std::shared_ptr<CheckpointObject>;
    struct Node {
        CheckpointPtr checkpoint;
        uint32_t frame = 0;
        Buttons p1;
        Buttons p2;
        std::vector<Event> path;
        float score = 0;
        uint64_t stateHash = 0;
    };

    auto captureCheckpoint = [&]() -> CheckpointPtr {
        auto checkpoint = playLayer->createCheckpoint();
        checkpoint->retain();
        return {checkpoint, [](CheckpointObject* value) { value->release(); }};
    };
    auto restore = [&](Node const& node, Buttons& p1, Buttons& p2) {
        for (uint8_t button = 1; button <= 3; ++button) {
            playLayer->handleButton(false, button, false);
            playLayer->handleButton(false, button, true);
        }
        playLayer->loadFromCheckpoint(node.checkpoint.get());
        auto apply = [&](Buttons const& buttons, bool player2) {
            if (buttons.jump) playLayer->handleButton(true, 1, player2);
            if (buttons.left) playLayer->handleButton(true, 2, player2);
            if (buttons.right) playLayer->handleButton(true, 3, player2);
        };
        apply(node.p1, false);
        apply(node.p2, true);
        p1 = node.p1;
        p2 = node.p2;
        s_collision = {};
        s_collision.active = true;
    };

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
            if (!object) continue;
            mix(static_cast<uint32_t>(object->m_uniqueID));
            mix(std::bit_cast<uint32_t>(object->getPositionX()));
            mix(std::bit_cast<uint32_t>(object->getPositionY()));
            mix(std::bit_cast<uint32_t>(object->getRotation()));
            mix(object->m_isDisabled | (object->m_isActivated << 1));
        }
        return hash;
    };

    std::mt19937 rng(runtimeSeed ^ 0x9e3779b9u);
    std::uniform_real_distribution<double> chance(0., 1.);
    std::vector<Node> frontier {{captureCheckpoint(), 0, {}, {}, {}, 0, 0}};
    std::vector<Event> solvedPath;

    for (int generation = 0;
         generation < maxGenerations && !stop && solvedPath.empty();
         ++generation) {
        std::vector<Node> candidates;
        std::unordered_set<uint64_t> seenStates;
        size_t deadBranches = 0;
        size_t attemptedBranches = 0;

        for (auto const& base : frontier) {
            auto ticks = pathfinder::fixedTickFrames(
                base.frame + 1, base.frame + horizon, physicsStep
            );
            for (int branch = 0; branch < branchesPerNode && !stop; ++branch) {
                ++attemptedBranches;
                struct Decision {
                    uint32_t frame;
                    bool p1Jump;
                    bool p2Jump;
                    int8_t p1Direction;
                    int8_t p2Direction;
                };
                std::vector<Decision> decisions;
                decisions.reserve(ticks.size());
                for (auto frame : ticks) {
                    decisions.push_back({
                        frame,
                        branch != 0 && chance(rng) < .10,
                        branch != 0 && chance(rng) < .10,
                        branch != 0 && chance(rng) < .06
                            ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1),
                        branch != 0 && chance(rng) < .06
                            ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1)
                    });
                }

                Buttons p1;
                Buttons p2;
                restore(base, p1, p2);
                std::vector<Event> events;
                size_t decisionIndex = 0;
                uint32_t releaseP1 = 0;
                uint32_t releaseP2 = 0;
                bool completed = false;

                auto send = [&](uint32_t frame, uint8_t button, bool player2,
                                bool down, bool& live) {
                    if (live == down) return;
                    playLayer->handleButton(down, button, player2);
                    live = down;
                    events.push_back({frame, button, player2, down});
                };
                auto jumpDecision = [&](uint32_t frame, bool player2, bool selected,
                                        PlayerObject* player, Buttons& buttons,
                                        uint32_t& releaseFrame) {
                    const bool action = player->m_isSpider || player->m_isSwing;
                    const bool harmless = player->m_isRobot || player->m_isBall ||
                        (!player->m_isShip && !player->m_isBird && !player->m_isDart &&
                         !player->m_isSpider && !player->m_isSwing);
                    const bool pulse = action ? selected : (branch == 1 && harmless);
                    if (pulse) {
                        send(frame, 1, player2, false, buttons.jump);
                        send(frame, 1, player2, true, buttons.jump);
                        releaseFrame = frame + 1;
                    } else if (selected) {
                        send(frame, 1, player2, !buttons.jump, buttons.jump);
                    }
                };
                auto directionDecision = [&](uint32_t frame, bool player2,
                                             int8_t direction, PlayerObject* player,
                                             Buttons& buttons) {
                    if (!player->m_isPlatformer || direction < 0) return;
                    send(frame, 2, player2, direction == 1, buttons.left);
                    send(frame, 3, player2, direction == 2, buttons.right);
                };

                for (uint32_t offset = 1; offset <= horizon; ++offset) {
                    const uint32_t frame = base.frame + offset;
                    if (releaseP1 == frame) {
                        send(frame, 1, false, false, p1.jump);
                        releaseP1 = 0;
                    }
                    if (releaseP2 == frame) {
                        send(frame, 1, true, false, p2.jump);
                        releaseP2 = 0;
                    }
                    if (decisionIndex < decisions.size() &&
                        decisions[decisionIndex].frame == frame) {
                        auto const decision = decisions[decisionIndex++];
                        jumpDecision(frame, false, decision.p1Jump,
                            playLayer->m_player1, p1, releaseP1);
                        jumpDecision(frame, true, decision.p2Jump,
                            playLayer->m_player2, p2, releaseP2);
                        directionDecision(frame, false, decision.p1Direction,
                            playLayer->m_player1, p1);
                        directionDecision(frame, true, decision.p2Direction,
                            playLayer->m_player2, p2);
                    }
                    playLayer->update(physicsStep);
                    if (playLayer->m_hasCompletedLevel) {
                        completed = true;
                        break;
                    }
                    if (s_collision.died) break;
                }

                if (completed) {
                    solvedPath = base.path;
                    solvedPath.insert(solvedPath.end(), events.begin(), events.end());
                } else if (!s_collision.died) {
                    const auto hash = runtimeStateHash();
                    if (seenStates.insert(hash).second) {
                        Node candidate;
                        candidate.checkpoint = captureCheckpoint();
                        candidate.frame = base.frame + horizon;
                        candidate.p1 = p1;
                        candidate.p2 = p2;
                        candidate.path = base.path;
                        candidate.path.insert(candidate.path.end(), events.begin(), events.end());
                        candidate.score = playLayer->getCurrentPercent() * 1000.f +
                            static_cast<float>(candidate.frame) * .001f;
                        candidate.stateHash = hash;
                        candidates.push_back(std::move(candidate));
                    }
                } else {
                    ++deadBranches;
                }

                co_yield true;
                if (!solvedPath.empty()) break;
            }
            if (!solvedPath.empty()) break;
        }

        if (!solvedPath.empty() || stop) break;
        if (candidates.empty()) {
            horizon = std::max(minHorizon, horizon / 2);
            continue;
        }

        std::sort(candidates.begin(), candidates.end(), [](Node const& a, Node const& b) {
            return a.score > b.score;
        });
        if (candidates.size() > beamWidth)
            candidates.resize(beamWidth);
        frontier = std::move(candidates);

        if (deadBranches * 2 > attemptedBranches)
            horizon = std::max(minHorizon, horizon / 2);
        else
            horizon = std::min(maxHorizon, horizon + 30);
        if (progress && !frontier.empty())
            progress(std::clamp<double>(frontier.front().score / 1000., 0., 100.));
    }

    if (solvedPath.empty())
        co_return std::vector<uint8_t>{};
    std::stable_sort(solvedPath.begin(), solvedPath.end(), [](auto const& a, auto const& b) {
        return a.frame < b.frame;
    });
    PathfinderReplay output;
    output.seed = static_cast<int>(runtimeSeed);
    for (auto const& event : solvedPath)
        output.inputs.emplace_back(event.frame, event.button, event.player2, event.down);
    co_return output.exportData().unwrapOr({});
}
