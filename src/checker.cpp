#include "checker.hpp"
#include "input_scheduler.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <gdr/gdr.hpp>
#include <algorithm>
#include <bit>
#include <chrono>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <string_view>
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

uint64_t levelChecksum(GJGameLevel* level) {
    auto data = ZipUtils::decompressString(level->m_levelString, true, 0);
    uint64_t hash = 1469598103934665603ull;
    auto const* bytes = reinterpret_cast<unsigned char const*>(data.c_str());
    for (size_t index = 0; index < data.size(); ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace

RuntimeVerificationTask verifyInGameCooperative(
    GJGameLevel* level,
    std::vector<uint8_t> macro,
    std::atomic_bool& stop
) {
    VerificationResult result;
    if (!level) {
        result.error = "missing level";
        co_return result;
    }
    if (macro.empty()) {
        result.error = "pathfinder returned an empty replay";
        co_return result;
    }

    auto imported = PathfinderReplay::importData(macro);
    if (imported.isErr()) {
        result.error = "could not decode generated replay";
        co_return result;
    }

    auto replay = imported.unwrap();
    constexpr std::string_view checksumPrefix = "level-checksum:";
    if (!replay.description.starts_with(checksumPrefix)) {
        result.error = "replay is missing the level checksum";
        co_return result;
    }
    uint64_t expectedChecksum = 0;
    try {
        expectedChecksum = std::stoull(
            replay.description.substr(checksumPrefix.size()), nullptr, 16);
    } catch (...) {
        result.error = "replay has an invalid level checksum";
        co_return result;
    }
    if (expectedChecksum != levelChecksum(level)) {
        result.error = "level changed after the path was searched";
        co_return result;
    }
    if (!std::isfinite(replay.framerate) || replay.framerate < 30. ||
        replay.framerate > 1000.) {
        result.error = "replay has an unsupported framerate";
        co_return result;
    }
    auto inputs = replay.inputs;
    const auto runtimeSeed = static_cast<uint32_t>(replay.seed);
    std::stable_sort(inputs.begin(), inputs.end(), [](auto const& lhs, auto const& rhs) {
        if (lhs.frame != rhs.frame) return lhs.frame < rhs.frame;
        if (lhs.player2 != rhs.player2) return lhs.player2 < rhs.player2;
        if (lhs.button != rhs.button) return lhs.button < rhs.button;
        return lhs.down < rhs.down; // release before press on the same frame
    });
    pathfinder::ClickRateLimiter clickLimiter(
        static_cast<uint64_t>(std::llround(replay.framerate)));
    for (auto const& input : inputs) {
        if (input.down && input.button == 1 &&
            !clickLimiter.accept(input.frame, input.player2)) {
            result.frame = static_cast<int>(input.frame);
            result.player2 = input.player2;
            result.error = "replay exceeds the 70 CPS cap";
            co_return result;
        }
    }
    std::deque<gdr::Input<"">> pending(inputs.begin(), inputs.end());

    // PlayLayer::create constructs the exact runtime object collection. Its
    // normal update runs spawn/move/toggle triggers and all 2.2 collision code.
    auto previousLayer = PlayLayer::get();
    CC_SAFE_RETAIN(previousLayer);
    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer) {
        CC_SAFE_RELEASE(previousLayer);
        result.error = "Geometry Dash could not create a verification PlayLayer";
        co_return result;
    }
    playLayer->retain();
    struct VerificationLayerLifetime {
        PlayLayer* layer;
        PlayLayer* previous;
        ~VerificationLayerLifetime() {
            layer->release();
            GameManager::sharedState()->m_playLayer = previous;
            CC_SAFE_RELEASE(previous);
        }
    } layerLifetime {playLayer, previousLayer};
    playLayer->setVisible(false);
    setRuntimeSeed(playLayer, runtimeSeed);
    playLayer->resetLevel();
    // Hidden PlayLayers never receive onEnterTransitionDidFinish, which is
    // where normal gameplay starts. Without this, update() advances triggers
    // but leaves both players frozen at X=0 and every search reports 0.00%.
    playLayer->startGame();

    CaptureGuard capture;
    const float step = 1.f / static_cast<float>(replay.framerate);
    // A malformed level must not run forever. Twenty minutes is above the
    // editor's practical duration while still providing a hard stop.
    const int maxFrames = static_cast<int>(20 * 60 * replay.framerate);

    for (int frame = 1; frame <= maxFrames && !stop; ++frame) {
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
        if (playLayer->m_gameState.m_isDualMode)
            mixPlayer(playLayer->m_player2);
        mix(playLayer->m_gameState.m_isDualMode);
        mix(playLayer->m_randomSeed);
        mix(playLayer->m_spawnTuples.size());
        mix(playLayer->m_sequenceTriggers.size());
        mix(playLayer->m_collectedItems ? playLayer->m_collectedItems->count() : 0);
        mix(playLayer->m_objectsToMove ? playLayer->m_objectsToMove->count() : 0);
        mix(playLayer->m_gameState.m_spawnChannelRelated0.size());
        mix(playLayer->m_gameState.m_spawnChannelRelated1.size());
        mix(playLayer->m_movedCount);
        mix(playLayer->m_areaMovedCount);
        result.frameHashes.push_back(result.traceHash);

        if (frame % 60 == 0)
            co_yield true;
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
        result.error = stop ? "verification cancelled" :
            "verification timed out before level completion";

    co_return result;
}

VerificationResult verifyInGame(GJGameLevel* level, std::vector<uint8_t> const& macro) {
    std::atomic_bool stop = false;
    auto task = verifyInGameCooperative(level, macro, stop);
    while (!task.resume()) {}
    return task.takeResult();
}

RuntimeSearchTask pathfindInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(SearchProgress const&)> progress
) {
    if (!level)
        co_return std::vector<uint8_t>{};

    auto previousLayer = PlayLayer::get();
    CC_SAFE_RETAIN(previousLayer);
    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer) {
        CC_SAFE_RELEASE(previousLayer);
        co_return std::vector<uint8_t>{};
    }
    playLayer->retain();
    struct LayerLifetime {
        PlayLayer* layer;
        PlayLayer* previous;
        ~LayerLifetime() {
            layer->release();
            GameManager::sharedState()->m_playLayer = previous;
            CC_SAFE_RELEASE(previous);
        }
    } layerLifetime {playLayer, previousLayer};
    playLayer->setVisible(false);
    playLayer->m_isSilent = true;

    const auto configuredSeed = Mod::get()->getSettingValue<int64_t>("search-seed");
    const uint32_t runtimeSeed = configuredSeed == 0
        ? (std::random_device{}() & 0x7fffffffu)
        : static_cast<uint32_t>(configuredSeed);
    log::info("Pathfinder runtime seed: {}", runtimeSeed);
    setRuntimeSeed(playLayer, runtimeSeed);
    playLayer->resetLevel();
    playLayer->startGame();
    CaptureGuard capture;

    const float searchStartX = playLayer->m_player1->getPositionX();
    const float searchEndX = playLayer->getEndPosition().x;
    auto runtimeProgress = [&] {
        const float gameProgress = playLayer->getCurrentPercent();
        if (gameProgress > 0.f || std::abs(searchEndX - searchStartX) < 1.f)
            return gameProgress;
        auto playerProgress = [&](PlayerObject* player) {
            return 100.f * (player->getPositionX() - searchStartX) /
                (searchEndX - searchStartX);
        };
        float fallback = playerProgress(playLayer->m_player1);
        if (playLayer->m_gameState.m_isDualMode)
            fallback = std::min(fallback, playerProgress(playLayer->m_player2));
        return std::clamp(fallback, 0.f, 100.f);
    };

    constexpr float physicsStep = 1.f / 240.f;
    constexpr uint32_t minHorizon = 60;
    constexpr uint32_t maxHorizon = 240;
    constexpr size_t maxBeamWidth = 6;
    constexpr int maxBranchesPerNode = 12;
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
    struct PathLink {
        std::shared_ptr<PathLink const> parent;
        std::vector<Event> events;
    };
    struct Node {
        CheckpointPtr checkpoint;
        uint32_t frame = 0;
        Buttons p1;
        Buttons p2;
        std::shared_ptr<PathLink const> path;
        float score = 0;
        uint64_t stateHash = 0;
        uint64_t diversityHash = 0;
    };
    struct GameplayEvent {
        GameObject* object = nullptr;
        float distance = std::numeric_limits<float>::infinity();
        std::string name = "open corridor";
    };

    auto eventName = [](GameObjectType type) -> std::string {
        switch (type) {
            case GameObjectType::Solid: return "solid";
            case GameObjectType::Hazard:
            case GameObjectType::AnimatedHazard: return "hazard";
            case GameObjectType::Slope: return "slope";
            case GameObjectType::DualPortal:
            case GameObjectType::SoloPortal: return "dual portal";
            case GameObjectType::TeleportPortal:
            case GameObjectType::TeleportOrb: return "teleport";
            case GameObjectType::DashRing:
            case GameObjectType::GravityDashRing: return "dash orb";
            case GameObjectType::Modifier: return "modifier";
            case GameObjectType::CollisionObject: return "collision object";
            default: return "orb or portal";
        }
    };
    auto findNextEvent = [&](PlayerObject* player) {
        GameplayEvent result;
        const float playerX = player->getPositionX();
        const bool movingLeft = player->m_isPlatformer &&
            player->m_platformerXVelocity < 0;
        for (auto* object : playLayer->m_activeObjects) {
            if (!object || object->m_isDisabled ||
                object->m_objectType == GameObjectType::Decoration ||
                object->m_objectType == GameObjectType::Collectible ||
                object->m_objectType == GameObjectType::UserCoin ||
                object->m_objectType == GameObjectType::SecretCoin)
                continue;
            const float delta = object->getPositionX() - playerX;
            if ((!movingLeft && delta < -15.f) || (movingLeft && delta > 15.f))
                continue;
            const float distance = std::abs(delta);
            if (distance < result.distance) {
                result.object = object;
                result.distance = distance;
                result.name = eventName(object->m_objectType);
            }
        }
        return result;
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
        if (playLayer->m_gameState.m_isDualMode)
            player(playLayer->m_player2);
        mix(playLayer->m_currentStep);
        mix(playLayer->m_gameState.m_commandIndex);
        mix(playLayer->m_randomSeed);
        mix(playLayer->m_spawnTuples.size());
        mix(playLayer->m_sequenceTriggers.size());
        mix(playLayer->m_collectedItems ? playLayer->m_collectedItems->count() : 0);
        mix(playLayer->m_objectsToMove ? playLayer->m_objectsToMove->count() : 0);
        mix(playLayer->m_gameState.m_spawnChannelRelated0.size());
        mix(playLayer->m_gameState.m_spawnChannelRelated1.size());
        mix(playLayer->m_movedCount);
        mix(playLayer->m_areaMovedCount);
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

    auto diversityHash = [&] {
        uint64_t hash = 1469598103934665603ull;
        auto mix = [&](int64_t value) {
            hash ^= static_cast<uint64_t>(value);
            hash *= 1099511628211ull;
        };
        auto player = [&](PlayerObject* value) {
            mix(static_cast<int64_t>(std::floor(value->getPositionX() / 30.f)));
            mix(static_cast<int64_t>(std::floor(value->getPositionY() / 30.f)));
            mix(static_cast<int64_t>(std::floor(value->m_yVelocity / 100.)));
            mix(value->m_isShip | (value->m_isBall << 1) |
                (value->m_isBird << 2) | (value->m_isDart << 3) |
                (value->m_isRobot << 4) | (value->m_isSpider << 5) |
                (value->m_isSwing << 6) | (value->m_isUpsideDown << 7));
        };
        player(playLayer->m_player1);
        if (playLayer->m_gameState.m_isDualMode) player(playLayer->m_player2);
        mix(playLayer->m_gameState.m_currentChannel);
        return hash;
    };

    std::mt19937 rng(runtimeSeed ^ 0x9e3779b9u);
    std::uniform_real_distribution<double> chance(0., 1.);
    auto rootCheckpoint = captureCheckpoint();
    std::vector<Node> frontier {{rootCheckpoint, 0, {}, {}, {}, 0, 0, 0}};
    std::vector<Event> solvedPath;
    int restart = 0;
    constexpr int maxRestarts = 3;
    uint64_t statesExpanded = 0;
    uint64_t physicsFrames = 0;
    auto searchStarted = std::chrono::steady_clock::now();
    std::string latestEvent = "start";

    for (int generation = 0;
         generation < maxGenerations && !stop && solvedPath.empty();
         ++generation) {
        std::vector<Node> candidates;
        std::unordered_set<uint64_t> seenStates;
        size_t deadBranches = 0;
        size_t attemptedBranches = 0;

        for (auto const& base : frontier) {
            Buttons eventP1;
            Buttons eventP2;
            restore(base, eventP1, eventP2);
            auto nextEvent = findNextEvent(playLayer->m_player1);
            latestEvent = nextEvent.name;
            const uint32_t eventFrames = nextEvent.object
                ? std::clamp<uint32_t>(
                    static_cast<uint32_t>(nextEvent.distance * .8f), 1, maxHorizon)
                : maxHorizon;
            const uint32_t nodeHorizon = std::clamp<uint32_t>(
                eventFrames + 45, minHorizon, horizon);
            const int branchesForNode = std::min(maxBranchesPerNode,
                nextEvent.distance < 300.f ? 12 : (frontier.size() == 1 ? 6 : 8));

            for (int branch = 0; branch < branchesForNode && !stop; ++branch) {
                ++attemptedBranches;
                ++statesExpanded;
                const bool baseHarmless = playLayer->m_player1->m_isRobot ||
                    playLayer->m_player1->m_isBall ||
                    (!playLayer->m_player1->m_isShip &&
                     !playLayer->m_player1->m_isBird &&
                     !playLayer->m_player1->m_isDart &&
                     !playLayer->m_player1->m_isSpider &&
                     !playLayer->m_player1->m_isSwing);
                // Branch one samples a deterministic rate between 1 and 70
                // CPS. Seventy is a strict ceiling, never a forced pattern.
                const int adaptiveCps = branch == 1
                    ? 1 + static_cast<int>(rng() % 70) : 0;
                uint32_t rolloutHorizon = nodeHorizon;
                if (branch == 1 && baseHarmless)
                    rolloutHorizon = std::max<uint32_t>(rolloutHorizon, 960);
                else if (branch == 0 && !nextEvent.object)
                    rolloutHorizon = std::max<uint32_t>(rolloutHorizon, 480);

                auto ticks = pathfinder::fixedTickFrames(
                    base.frame + 1, base.frame + rolloutHorizon, physicsStep
                );
                const uint32_t targetFrame = base.frame +
                    (eventFrames > 60 ? eventFrames - 60 : 1);
                const auto endPosition = playLayer->getEndPosition();
                const int8_t p1TowardEnd = endPosition.x <
                    playLayer->m_player1->getPositionX() ? 1 : 2;
                const int8_t p2TowardEnd = endPosition.x <
                    playLayer->m_player2->getPositionX() ? 1 : 2;
                struct Decision {
                    uint32_t frame;
                    bool p1Jump;
                    bool p2Jump;
                    int8_t p1Direction;
                    int8_t p2Direction;
                };
                std::vector<Decision> decisions;
                decisions.reserve(ticks.size());
                for (size_t tickIndex = 0; tickIndex < ticks.size(); ++tickIndex) {
                    const auto frame = ticks[tickIndex];
                    auto templateJump = [&](bool player2) {
                        if (branch == 0) return false;
                        if (branch == 1)
                            // Divide by 71 so even a 70-CPS target remains an
                            // adaptive pattern rather than forced every-tick spam.
                            return chance(rng) < static_cast<double>(adaptiveCps) / 71.;
                        if (branch == 2) return tickIndex == 0;
                        if (branch == 3)
                            return frame >= targetFrame &&
                                (tickIndex == 0 || ticks[tickIndex - 1] < targetFrame);
                        if (branch == 4) {
                            const uint32_t releaseTarget = targetFrame + 20;
                            return (frame >= targetFrame &&
                                    (tickIndex == 0 || ticks[tickIndex - 1] < targetFrame)) ||
                                   (frame >= releaseTarget &&
                                    (tickIndex == 0 || ticks[tickIndex - 1] < releaseTarget));
                        }
                        const double probability =
                            std::abs(static_cast<int64_t>(frame) - targetFrame) < 90
                                ? .18 : .04;
                        (void)player2;
                        return chance(rng) < probability;
                    };
                    decisions.push_back({
                        frame,
                        templateJump(false),
                        templateJump(true),
                        (branch == 2 || branch == 3) && tickIndex == 0
                            ? p1TowardEnd
                            : (branch >= 5 && chance(rng) < .06
                                ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1)),
                        (branch == 2 || branch == 3) && tickIndex == 0
                            ? p2TowardEnd
                            : (branch >= 5 && chance(rng) < .06
                                ? static_cast<int8_t>(rng() % 3) : static_cast<int8_t>(-1))
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
                    if (player2 && !playLayer->m_gameState.m_isDualMode) return;
                    const bool action = player->m_isSpider || player->m_isSwing;
                    const bool harmless = player->m_isRobot || player->m_isBall ||
                        (!player->m_isShip && !player->m_isBird && !player->m_isDart &&
                         !player->m_isSpider && !player->m_isSwing);
                    const bool pulseCandidate = branch == 1 || branch >= 5;
                    const bool pulse = selected &&
                        (action || (pulseCandidate && harmless));
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
                    if ((player2 && !playLayer->m_gameState.m_isDualMode) ||
                        !player->m_isPlatformer || direction < 0) return;
                    send(frame, 2, player2, direction == 1, buttons.left);
                    send(frame, 3, player2, direction == 2, buttons.right);
                };

                for (uint32_t offset = 1; offset <= rolloutHorizon; ++offset) {
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
                    ++physicsFrames;
                    if (playLayer->m_hasCompletedLevel) {
                        completed = true;
                        break;
                    }
                    if (s_collision.died) break;
                    if (offset % 240 == 0)
                        co_yield true;
                }

                if (completed) {
                    auto leaf = std::make_shared<PathLink const>(PathLink {
                        base.path, std::move(events)
                    });
                    std::vector<std::shared_ptr<PathLink const>> links;
                    for (auto link = leaf; link; link = link->parent)
                        links.push_back(link);
                    for (auto it = links.rbegin(); it != links.rend(); ++it)
                        solvedPath.insert(solvedPath.end(),
                            (*it)->events.begin(), (*it)->events.end());
                } else if (!s_collision.died) {
                    const auto hash = runtimeStateHash();
                    if (seenStates.insert(hash).second) {
                        Node candidate;
                        candidate.checkpoint = captureCheckpoint();
                        candidate.frame = base.frame + rolloutHorizon;
                        candidate.p1 = p1;
                        candidate.p2 = p2;
                        candidate.path = std::make_shared<PathLink const>(PathLink {
                            base.path, std::move(events)
                        });
                        const auto end = playLayer->getEndPosition();
                        const auto p1pos = playLayer->m_player1->getPosition();
                        const float endDistance = std::hypot(
                            p1pos.x - end.x, p1pos.y - end.y);
                        float velocityPenalty = static_cast<float>(
                            std::abs(playLayer->m_player1->m_yVelocity) * .01);
                        if (playLayer->m_gameState.m_isDualMode)
                            velocityPenalty += static_cast<float>(
                                std::abs(playLayer->m_player2->m_yVelocity) * .01);
                        candidate.score = runtimeProgress() * 1000.f -
                            endDistance * .001f - velocityPenalty -
                            static_cast<float>(candidate.path->events.size()) * .1f;
                        candidate.stateHash = hash;
                        candidate.diversityHash = diversityHash();
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
            if (horizon == minHorizon && restart < maxRestarts) {
                ++restart;
                rng.seed((runtimeSeed ^ 0x9e3779b9u) +
                    static_cast<uint32_t>(restart) * 0x85ebca6bu);
                frontier = {{rootCheckpoint, 0, {}, {}, {}, 0, 0, 0}};
                horizon = 120;
                log::info("Pathfinder deterministic restart {}/{} (seed {})",
                    restart, maxRestarts, runtimeSeed);
            } else {
                horizon = std::max(minHorizon, horizon / 2);
            }
            continue;
        }

        std::sort(candidates.begin(), candidates.end(), [](Node const& a, Node const& b) {
            return a.score > b.score;
        });
        const size_t targetBeamWidth = deadBranches * 2 > attemptedBranches
            ? maxBeamWidth
            : (latestEvent == "open corridor" ? 1u : 3u);
        std::unordered_set<uint64_t> diversityBuckets;
        std::vector<Node> diverse;
        diverse.reserve(targetBeamWidth);
        for (auto& candidate : candidates) {
            if (diversityBuckets.insert(candidate.diversityHash).second) {
                diverse.push_back(std::move(candidate));
                if (diverse.size() == targetBeamWidth) break;
            }
        }
        // If clustering was too aggressive, fill remaining slots by score.
        for (auto& candidate : candidates) {
            if (diverse.size() == targetBeamWidth) break;
            if (candidate.checkpoint)
                diverse.push_back(std::move(candidate));
        }
        frontier = std::move(diverse);

        if (deadBranches * 2 > attemptedBranches)
            horizon = std::max(minHorizon, horizon / 2);
        else
            horizon = std::min(maxHorizon, horizon + 30);
        if (progress && !frontier.empty()) {
            Buttons statusP1;
            Buttons statusP2;
            restore(frontier.front(), statusP1, statusP2);
            auto modeName = [](PlayerObject* player) -> std::string {
                if (player->m_isSpider) return "Spider";
                if (player->m_isSwing) return "Swing";
                if (player->m_isRobot) return "Robot";
                if (player->m_isDart) return "Wave";
                if (player->m_isBird) return "UFO";
                if (player->m_isBall) return "Ball";
                if (player->m_isShip) return "Ship";
                return "Cube";
            };
            SearchProgress status;
            status.percent = std::clamp<double>(runtimeProgress(), 0., 100.);
            status.generation = generation + 1;
            status.restart = restart;
            status.horizon = horizon;
            status.beamSize = frontier.size();
            status.statesExpanded = statesExpanded;
            status.physicsFrames = physicsFrames;
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - searchStarted).count();
            status.updatesPerSecond = elapsed > 0
                ? static_cast<double>(physicsFrames) / elapsed : 0;
            status.nextEvent = latestEvent;
            status.player1Mode = modeName(playLayer->m_player1);
            status.player2Mode = playLayer->m_gameState.m_isDualMode
                ? modeName(playLayer->m_player2) : "-";
            progress(status);
        }
    }

    if (solvedPath.empty())
        co_return std::vector<uint8_t>{};
    std::stable_sort(solvedPath.begin(), solvedPath.end(), [](auto const& a, auto const& b) {
        if (a.frame != b.frame) return a.frame < b.frame;
        if (a.player2 != b.player2) return a.player2 < b.player2;
        if (a.button != b.button) return a.button < b.button;
        return a.down < b.down;
    });
    // Remove state-neutral records while preserving release-before-press pulse
    // ordering. Runtime verification still gates the minimized replay.
    bool buttonState[2][4] {};
    std::vector<Event> minimized;
    minimized.reserve(solvedPath.size());
    for (auto const& event : solvedPath) {
        auto& state = buttonState[event.player2 ? 1 : 0][event.button];
        if (state == event.down) continue;
        state = event.down;
        minimized.push_back(event);
    }

    PathfinderReplay output;
    output.seed = static_cast<int>(runtimeSeed);
    output.framerate = 240.;
    output.description = fmt::format("level-checksum:{:016x}", levelChecksum(level));
    for (auto const& event : minimized)
        output.inputs.emplace_back(event.frame, event.button, event.player2, event.down);
    co_return output.exportData().unwrapOr({});
}

RuntimeSearchTask pathfindRollingInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(SearchProgress const&)> progress
) {
    if (!level)
        co_return std::vector<uint8_t>{};

    auto previousLayer = PlayLayer::get();
    CC_SAFE_RETAIN(previousLayer);
    auto playLayer = PlayLayer::create(level, false, false);
    if (!playLayer) {
        CC_SAFE_RELEASE(previousLayer);
        co_return std::vector<uint8_t>{};
    }
    playLayer->retain();
    struct LayerLifetime {
        PlayLayer* layer;
        PlayLayer* previous;
        ~LayerLifetime() {
            layer->release();
            GameManager::sharedState()->m_playLayer = previous;
            CC_SAFE_RELEASE(previous);
        }
    } lifetime {playLayer, previousLayer};

    playLayer->setVisible(false);
    playLayer->m_isSilent = true;
    const auto configuredSeed = Mod::get()->getSettingValue<int64_t>("search-seed");
    const uint32_t runtimeSeed = configuredSeed == 0
        ? (std::random_device{}() & 0x7fffffffu)
        : static_cast<uint32_t>(configuredSeed);
    setRuntimeSeed(playLayer, runtimeSeed);
    playLayer->resetLevel();
    playLayer->startGame();
    CaptureGuard capture;

    constexpr float physicsStep = 1.f / 240.f;
    constexpr uint32_t horizon = 1000;
    constexpr int iterations = 64;
    struct Event { uint32_t frame; uint8_t button; bool player2; bool down; };
    struct Controls { bool p1 = false; bool p2 = false; };
    using CheckpointPtr = std::shared_ptr<CheckpointObject>;
    struct PathLink {
        std::shared_ptr<PathLink const> parent;
        std::vector<Event> events;
    };
    struct State {
        CheckpointPtr checkpoint;
        uint32_t frame = 0;
        Controls controls;
        std::shared_ptr<PathLink const> path;
    };
    struct Trial {
        std::vector<Event> events;
        uint32_t survived = 0;
        Controls controls;
        bool completed = false;
    };

    auto checkpoint = [&]() -> CheckpointPtr {
        auto value = playLayer->createCheckpoint();
        value->retain();
        return {value, [](CheckpointObject* object) { object->release(); }};
    };
    auto restore = [&](State const& state, Controls& controls) {
        playLayer->handleButton(false, 1, false);
        playLayer->handleButton(false, 1, true);
        playLayer->loadFromCheckpoint(state.checkpoint.get());
        if (state.controls.p1) playLayer->handleButton(true, 1, false);
        if (state.controls.p2) playLayer->handleButton(true, 1, true);
        controls = state.controls;
        s_collision = {};
        s_collision.active = true;
    };
    auto flatten = [](std::shared_ptr<PathLink const> link) {
        std::vector<std::shared_ptr<PathLink const>> links;
        for (; link; link = link->parent) links.push_back(link);
        std::vector<Event> result;
        for (auto it = links.rbegin(); it != links.rend(); ++it)
            result.insert(result.end(), (*it)->events.begin(), (*it)->events.end());
        return result;
    };

    const float startX = playLayer->m_player1->getPositionX();
    const float endX = playLayer->getEndPosition().x;
    auto currentProgress = [&] {
        float value = playLayer->getCurrentPercent();
        if (value <= 0.f && std::abs(endX - startX) > 1.f)
            value = 100.f * (playLayer->m_player1->getPositionX() - startX) /
                (endX - startX);
        return std::clamp(value, 0.f, 100.f);
    };

    std::mt19937 rng(runtimeSeed ^ 0x9e3779b9u);
    std::vector<State> history {{checkpoint(), 0, {}, {}}};
    int failures = 0;
    uint64_t expanded = 0;
    uint64_t physicsFrames = 0;
    auto started = std::chrono::steady_clock::now();
    std::vector<Event> solution;

    while (!stop && solution.empty()) {
        State base = history.back();
        Trial best;
        for (int iteration = 0; iteration < iterations && !stop; ++iteration) {
            Controls controls;
            restore(base, controls);
            Trial trial;
            trial.controls = controls;
            auto ticks = pathfinder::fixedTickFrames(
                base.frame + 1, base.frame + horizon, physicsStep);
            std::unordered_set<uint32_t> selected;
            if (iteration != 0 && !ticks.empty()) {
                const size_t choices = std::min<size_t>(30, ticks.size());
                std::uniform_int_distribution<size_t> pick(0, ticks.size() - 1);
                while (selected.size() < choices)
                    selected.insert(ticks[pick(rng)]);
            }
            size_t tickIndex = 0;
            uint32_t releaseP1 = 0;
            uint32_t releaseP2 = 0;
            auto send = [&](uint32_t frame, bool player2, bool down) {
                bool& live = player2 ? trial.controls.p2 : trial.controls.p1;
                if (live == down) return;
                playLayer->handleButton(down, 1, player2);
                live = down;
                trial.events.push_back({frame, 1, player2, down});
            };
            auto act = [&](uint32_t frame, bool player2, PlayerObject* player) {
                if (player2 && !playLayer->m_gameState.m_isDualMode) return;
                if (player->m_isSpider || player->m_isSwing) {
                    send(frame, player2, false);
                    send(frame, player2, true);
                    (player2 ? releaseP2 : releaseP1) = frame + 1;
                } else {
                    const bool live = player2 ? trial.controls.p2 : trial.controls.p1;
                    send(frame, player2, !live);
                }
            };

            for (uint32_t offset = 1; offset <= horizon; ++offset) {
                const uint32_t frame = base.frame + offset;
                if (releaseP1 == frame) { send(frame, false, false); releaseP1 = 0; }
                if (releaseP2 == frame) { send(frame, true, false); releaseP2 = 0; }
                if (tickIndex < ticks.size() && ticks[tickIndex] == frame) {
                    if (selected.contains(frame)) {
                        act(frame, false, playLayer->m_player1);
                        if (playLayer->m_gameState.m_isDualMode && (rng() & 1))
                            act(frame, true, playLayer->m_player2);
                    }
                    ++tickIndex;
                }
                playLayer->update(physicsStep);
                ++physicsFrames;
                trial.survived = offset;
                if (playLayer->m_hasCompletedLevel) {
                    trial.completed = true;
                    break;
                }
                if (s_collision.died) break;
                if (offset % 240 == 0) co_yield true;
            }
            ++expanded;
            if (trial.completed || trial.survived > best.survived)
                best = std::move(trial);
            co_yield true;
            if (best.completed || (best.survived > 500 && failures < 4)) break;
        }

        if (best.completed) {
            auto leaf = std::make_shared<PathLink const>(PathLink {
                base.path, std::move(best.events)
            });
            solution = flatten(leaf);
            break;
        }
        if (best.survived < 8) {
            ++failures;
            if (history.size() > 1) history.pop_back();
            if (failures > 20) {
                rng.seed((runtimeSeed ^ 0x9e3779b9u) + failures * 0x85ebca6bu);
                failures = 0;
            }
            continue;
        }

        const uint32_t advance = std::max<uint32_t>(1, best.survived * 2 / 3);
        Controls committedControls;
        restore(base, committedControls);
        std::vector<Event> committedEvents;
        size_t eventIndex = 0;
        for (uint32_t offset = 1; offset <= advance; ++offset) {
            const uint32_t frame = base.frame + offset;
            while (eventIndex < best.events.size() &&
                   best.events[eventIndex].frame == frame) {
                auto const event = best.events[eventIndex++];
                playLayer->handleButton(event.down, event.button, event.player2);
                (event.player2 ? committedControls.p2 : committedControls.p1) = event.down;
                committedEvents.push_back(event);
            }
            playLayer->update(physicsStep);
            ++physicsFrames;
            if (s_collision.died) break;
        }
        if (s_collision.died) {
            ++failures;
            continue;
        }
        auto link = std::make_shared<PathLink const>(PathLink {
            base.path, std::move(committedEvents)
        });
        history.push_back({checkpoint(), base.frame + advance, committedControls, link});
        failures = 0;

        if (progress) {
            SearchProgress status;
            status.percent = currentProgress();
            status.generation = static_cast<int>(history.size() - 1);
            status.beamSize = 1;
            status.horizon = horizon;
            status.statesExpanded = expanded;
            status.physicsFrames = physicsFrames;
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            status.updatesPerSecond = elapsed > 0 ? physicsFrames / elapsed : 0;
            status.nextEvent = "rolling random search";
            status.player1Mode = "runtime";
            status.player2Mode = playLayer->m_gameState.m_isDualMode ? "runtime" : "-";
            progress(status);
        }
    }

    if (solution.empty()) co_return std::vector<uint8_t>{};
    std::stable_sort(solution.begin(), solution.end(), [](auto const& a, auto const& b) {
        if (a.frame != b.frame) return a.frame < b.frame;
        if (a.player2 != b.player2) return a.player2 < b.player2;
        return a.down < b.down;
    });
    PathfinderReplay output;
    output.seed = static_cast<int>(runtimeSeed);
    output.framerate = 240.;
    output.description = fmt::format("level-checksum:{:016x}", levelChecksum(level));
    for (auto const& event : solution)
        output.inputs.emplace_back(event.frame, event.button, event.player2, event.down);
    co_return output.exportData().unwrapOr({});
}
