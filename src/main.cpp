#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "checker.hpp"
#include <chrono>
#include <optional>
#include <sstream>

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    SearchProgress m_searchProgress;
    bool m_started = false;
    bool m_finished = false;
    std::optional<RuntimeSearchTask> m_search;
    std::optional<RuntimeVerificationTask> m_verification;
    std::optional<VerificationResult> m_firstVerification;
    std::vector<uint8_t> m_pendingMacro;
    std::string m_levelName;
    GJGameLevel* m_level = nullptr;
public:
    static PathfinderNode* create(GJGameLevel* level, std::string const& levelName, std::string const& lvlString) {
        auto node = new PathfinderNode();
        if (node && node->init(level, levelName, lvlString)) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    ~PathfinderNode() {
        m_stop = true;
        m_search.reset();
        m_verification.reset();
        CC_SAFE_RELEASE(m_level);
    }

    void finalize(std::vector<uint8_t> macro, VerificationResult verification) {
        m_finished = true;
        getChildByIDRecursive("stop")->setVisible(false);

        if (!verification.completed) {
            if (verification.died) {
                log::error(
                    "Path rejected: P{} collision at frame {}; object ID {} at "
                    "({:.3f}, {:.3f}), rotation {:.3f}, scale ({:.3f}, {:.3f}); "
                    "player ({:.3f}, {:.3f}), y-velocity {:.6f}",
                    verification.player2 ? 2 : 1, verification.frame,
                    verification.objectID, verification.objectX, verification.objectY,
                    verification.objectRotation, verification.objectScaleX,
                    verification.objectScaleY, verification.playerX,
                    verification.playerY, verification.playerYVelocity
                );
            } else {
                log::error("Path rejected at frame {}: {}", verification.frame, verification.error);
            }

            const auto reason = verification.died
                ? fmt::format(
                    "The runtime-searched path died during fresh verification.\n"
                    "P{}  Frame: {}  Object: {}\n"
                    "Object: ({:.1f}, {:.1f}) rot {:.1f}\n"
                    "Player: ({:.1f}, {:.1f})\n\n"
                    "The unsafe replay was not exported.",
                    verification.player2 ? 2 : 1, verification.frame,
                    verification.objectID, verification.objectX,
                    verification.objectY, verification.objectRotation,
                    verification.playerX, verification.playerY
                )
                : fmt::format(
                    "The path could not be verified in Geometry Dash.\n{}\n\n"
                    "The unsafe replay was not exported.", verification.error
                );
            FLAlertLayer::create("Path rejected", reason, "OK")->show();
            removeFromParentAndCleanup(true);
            return;
        }

        auto callback = [this, macro](this auto self) -> arc::Future<void> {
            auto saveDir = Mod::get()->getSaveDir();
            if (Loader::get()->isModLoaded("eclipse.eclipse-menu")) {
                saveDir = Loader::get()->getLoadedMod("eclipse.eclipse-menu")->getSaveDir() / "replays";
            }

            if (!exists(saveDir)) {
                create_directories(saveDir);
            }

            FilePickOptions opts(
                saveDir / fmt::format("{}.gdr2", m_levelName), {{
                std::string("Macro File"),
                std::unordered_set {std::string("gdr2")}
            }});

            if (auto path = co_await pick(PickMode::SaveFile, opts); path.isOk() && path.unwrap().has_value()) {
                (void)writeBinary(*path.unwrap(), macro);
                queueInMainThread([this] {
                    removeFromParentAndCleanup(true);
                });
            }
        };

        Build<ButtonSprite>::create("Export", "bigFont.fnt", "GJ_button_01.png")
            .intoMenuItem(async::wrapSpawn(callback))
            .scale(0.8)
            .move(0, -40)
            .parent(getChildByID("menu"));
    }

    void keyBackClicked() override  {
        m_stop = true;
        CCLayer::keyBackClicked();
        removeFromParentAndCleanup(true);
    }

    bool init(GJGameLevel* level, std::string const& levelName, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_level = level;
        CC_SAFE_RETAIN(m_level);
        m_levelName = levelName;

        (void)lvlString; // Runtime PlayLayer is now the authoritative level source.
        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
            Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                .string(fmt::format(
                    "{:.2f}% | G{} B{} H{} R{} | {}/{}",
                    m_progress.load(), m_searchProgress.generation,
                    m_searchProgress.beamSize, m_searchProgress.horizon,
                    m_searchProgress.restart, m_searchProgress.player1Mode,
                    m_searchProgress.player2Mode
                ).c_str());
            if (m_finished)
                return;

            try {
                if (!m_started) {
                    m_started = true;
                    m_search.emplace(pathfindInGame(m_level, m_stop,
                        [this](SearchProgress const& status) {
                            m_searchProgress = status;
                            if (m_progress < status.percent)
                                m_progress = status.percent;
                        }));
                }

                // Search and verification both stay on the cocos thread, but
                // only consume a small frame budget so controls remain live.
                const auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(4);
                if (m_search) {
                    bool done = false;
                    do {
                        done = m_search->resume();
                    } while (!done && std::chrono::steady_clock::now() < deadline);
                    if (done) {
                        m_pendingMacro = m_search->takeResult();
                        m_search.reset();
                        m_verification.emplace(verifyInGameCooperative(
                            m_level, m_pendingMacro, m_stop));
                    }
                    return;
                }

                if (m_verification) {
                    bool done = false;
                    do {
                        done = m_verification->resume();
                    } while (!done && std::chrono::steady_clock::now() < deadline);
                    if (!done)
                        return;

                    auto result = m_verification->takeResult();
                    m_verification.reset();
                    if (!m_firstVerification && result.completed) {
                        m_firstVerification = result;
                        m_verification.emplace(verifyInGameCooperative(
                            m_level, m_pendingMacro, m_stop));
                        return;
                    }
                    if (m_firstVerification && result.completed &&
                        m_firstVerification->traceHash != result.traceHash) {
                        const auto firstHash = m_firstVerification->traceHash;
                        const auto count = std::min(
                            m_firstVerification->frameHashes.size(), result.frameHashes.size());
                        size_t mismatch = 0;
                        while (mismatch < count &&
                            m_firstVerification->frameHashes[mismatch] == result.frameHashes[mismatch])
                            ++mismatch;
                        result.frame = static_cast<int>(mismatch + 1);
                        result.completed = false;
                        result.error = fmt::format(
                            "runtime trace mismatch at frame {} ({:016x} != {:016x})",
                            result.frame, firstHash, result.traceHash);
                    }
                    finalize(std::move(m_pendingMacro), std::move(result));
                }
            } catch (std::exception const& error) {
                log::error("Runtime pathfinder failed: {}", error.what());
                m_search.reset();
                m_verification.reset();
                VerificationResult failure;
                failure.error = error.what();
                finalize({}, std::move(failure));
            }
        });

        auto handle = [this](CCMenuItemSpriteExtra* it) {
            m_stop = true;

            // A stop request rejects the incomplete candidate rather than
            // exporting an unverified replay.
            removeFromParentAndCleanup(true);
        };

        auto menu = Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(250, 140),
            Build<CCLabelBMFont>::create("Pathfinding...", "bigFont.fnt")
                .move(0, 50)
                .scale(0.8),
            Build<CCLabelBMFont>::create("0.00", "chatFont.fnt")
                .id("percent")
                .move(0, 10),
            Build<ButtonSprite>::create("Stop", "bigFont.fnt", "GJ_button_04.png")
                .scale(0.8)
                .intoMenuItem(handle)
                .id("stop")
                .move(0, -40),
            Build<CCSprite>::createSpriteName("GJ_closeBtn_001.png")
                .intoMenuItem(handle)
                .id("close")
                .move(-125, 70)
                .scale(0.8)
        );

        /*    .intoNewChild(CCMenu::create())
                .id("menu")
                .intoNewChild(CCScale9Sprite::create("GJ_square04.png"))
                    .contentSize(250, 140)
                .intoNewSibling(CCLabelBMFont::create("Pathfinding...", "bigFont.fnt"))
                    .move(0, 50)
                    .scale(0.8)
                .intoNewSibling(CCLabelBMFont::create("0.00", "chatFont.fnt"))
                    .id("percent")
                    .move(0, 10)
                .intoNewSibling(ButtonSprite::create("Stop", "bigFont.fnt", "GJ_button_04.png"))
                    .intoMenuItem([this]() {
                        m_stop = true;
                        finalize(m_result.get());
                    })
                    .scale(0.8)
                    .id("cancel")
                    .move(0, -40)
                .intoNewSibling(CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"))
                    .intoMenuItem([this]() {
                        m_stop = true;
                        this->removeFromParentAndCleanup(true);
                    })
                    .move(-125, 70)
                    .scale(0.8);*/
        ;

        return true;
    }

};


class AcceptanceRunnerNode : public CCLayerColor {
    std::vector<std::filesystem::path> m_fixtures;
    size_t m_index = 0;
    std::optional<RuntimeSearchTask> m_search;
    std::optional<RuntimeVerificationTask> m_verification;
    std::optional<VerificationResult> m_firstVerification;
    std::vector<uint8_t> m_macro;
    GJGameLevel* m_level = nullptr;
    std::atomic_bool m_stop = false;
    std::atomic_bool m_fixtureStop = false;
    double m_levelProgress = 0;
    SearchProgress m_fixtureSearchProgress;
    std::chrono::steady_clock::time_point m_fixtureStarted;
    std::stringstream m_report;
    std::stringstream m_json;
    bool m_firstJson = true;

    void releaseLevel() {
        CC_SAFE_RELEASE_NULL(m_level);
    }

    static std::string jsonEscape(std::string value) {
        for (size_t pos = 0; (pos = value.find_first_of("\\\"\n", pos)) != std::string::npos; ) {
            const char ch = value[pos];
            value.replace(pos, 1, ch == '\n' ? "\\n" : (ch == '\"' ? "\\\"" : "\\\\"));
            pos += 2;
        }
        return value;
    }

    void recordResult(std::string const& status, VerificationResult const& result) {
        const auto name = m_fixtures[m_index].filename().string();
        const auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - m_fixtureStarted).count();
        m_report << name << ": " << status
                 << " frame=" << result.frame
                 << " object=" << result.objectID
                 << " position=(" << result.objectX << ',' << result.objectY << ')'
                 << " error=" << result.error << '\n';
        if (!m_firstJson) m_json << ',';
        m_firstJson = false;
        m_json << "{\"fixture\":\"" << jsonEscape(name)
               << "\",\"status\":\"" << status
               << "\",\"configuredSearchSeed\":"
               << Mod::get()->getSettingValue<int64_t>("search-seed")
               << ",\"seconds\":" << elapsed
               << ",\"generation\":" << m_fixtureSearchProgress.generation
               << ",\"restart\":" << m_fixtureSearchProgress.restart
               << ",\"beamSize\":" << m_fixtureSearchProgress.beamSize
               << ",\"horizon\":" << m_fixtureSearchProgress.horizon
               << ",\"frame\":" << result.frame
               << ",\"objectID\":" << result.objectID
               << ",\"objectX\":" << result.objectX
               << ",\"objectY\":" << result.objectY
               << ",\"traceHash\":\"" << fmt::format("{:016x}", result.traceHash)
               << "\",\"error\":\"" << jsonEscape(result.error) << "\"}";

        if (status != "PASS") {
            const auto dir = Mod::get()->getSaveDir() / "diagnostics" /
                m_fixtures[m_index].stem();
            create_directories(dir);
            if (!m_macro.empty()) (void)writeBinary(dir / "candidate.gdr2", m_macro);
            if (auto raw = readString(m_fixtures[m_index]); raw.isOk())
                (void)writeString(dir / "level.lvl", raw.unwrap());
            (void)writeString(dir / "failure.txt", result.error);
        }
    }

    void finish() {
        m_fixtureStop = true;
        m_search.reset();
        m_verification.reset();
        releaseLevel();
        auto reportPath = Mod::get()->getSaveDir() / "acceptance-report.txt";
        (void)writeString(reportPath, m_report.str());
        m_json << "],\"tested\":" << m_index
               << ",\"total\":" << m_fixtures.size() << '}';
        (void)writeString(Mod::get()->getSaveDir() / "acceptance-report.json",
            m_json.str());
        const bool stopped = m_stop;
        FLAlertLayer::create(
            stopped ? "Acceptance stopped" : "Acceptance complete",
            fmt::format("Tested {}/{} fixtures.\nReport: {}",
                m_index, m_fixtures.size(), reportPath.string()),
            "OK"
        )->show();
        removeFromParentAndCleanup(true);
    }

    bool startFixture() {
        if (m_index >= m_fixtures.size())
            return false;
        m_fixtureStarted = std::chrono::steady_clock::now();
        auto raw = readString(m_fixtures[m_index]);
        if (raw.isErr()) {
            VerificationResult result;
            result.error = "fixture could not be read";
            recordResult("ERROR", result);
            ++m_index;
            return true;
        }

        m_level = GJGameLevel::create();
        m_level->retain();
        m_level->m_levelName = m_fixtures[m_index].stem().string();
        m_level->m_levelString = ZipUtils::compressString(raw.unwrap(), true, 0);
        m_levelProgress = 0;
        m_fixtureSearchProgress = {};
        m_macro.clear();
        m_firstVerification.reset();
        m_fixtureStop = false;
        m_fixtureStarted = std::chrono::steady_clock::now();
        m_search.emplace(pathfindInGame(m_level, m_fixtureStop,
            [this](SearchProgress const& status) {
                m_fixtureSearchProgress = status;
                m_levelProgress = std::max(m_levelProgress, status.percent);
            }));
        return true;
    }

    void step(float) {
        if (m_stop) {
            finish();
            return;
        }
        if (!m_search && !m_verification && !startFixture()) {
            finish();
            return;
        }
        const auto timeout = Mod::get()->getSettingValue<int64_t>("test-timeout");
        if ((m_search || m_verification) &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                m_fixtureStarted).count() > static_cast<double>(timeout)) {
            m_fixtureStop = true;
            m_search.reset();
            m_verification.reset();
            VerificationResult result;
            result.error = "fixture timed out";
            recordResult("TIMEOUT", result);
            ++m_index;
            releaseLevel();
            return;
        }
        auto label = typeinfo_cast<CCLabelBMFont*>(getChildByID("status"));
        if (label) {
            const auto elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_fixtureStarted).count();
            label->setString(fmt::format(
                "{}/{} {} | {} | {:.1f}% | G{} B{} H{} | {}/{} | {:.1f}s",
                m_index + 1, m_fixtures.size(),
                m_fixtures[m_index].stem().string(),
                m_verification ? "Verify" : "Search", m_levelProgress,
                m_fixtureSearchProgress.generation,
                m_fixtureSearchProgress.beamSize,
                m_fixtureSearchProgress.horizon,
                m_fixtureSearchProgress.player1Mode,
                m_fixtureSearchProgress.player2Mode, elapsed
            ).c_str());
        }

        try {
            const auto deadline = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(4);
            if (m_search) {
                bool done = false;
                do {
                    done = m_search->resume();
                } while (!done && std::chrono::steady_clock::now() < deadline);
                if (done) {
                    m_macro = m_search->takeResult();
                    m_search.reset();
                    m_verification.emplace(verifyInGameCooperative(
                        m_level, m_macro, m_fixtureStop));
                }
                return;
            }

            if (m_verification) {
                bool done = false;
                do {
                    done = m_verification->resume();
                } while (!done && std::chrono::steady_clock::now() < deadline);
                if (!done) return;

                auto result = m_verification->takeResult();
                m_verification.reset();
                if (!m_firstVerification && result.completed) {
                    m_firstVerification = result;
                    m_verification.emplace(verifyInGameCooperative(
                        m_level, m_macro, m_fixtureStop));
                    return;
                }
                const bool pass = result.completed && m_firstVerification &&
                    result.traceHash == m_firstVerification->traceHash;
                if (result.completed && m_firstVerification && !pass) {
                    const auto count = std::min(
                        m_firstVerification->frameHashes.size(), result.frameHashes.size());
                    size_t mismatch = 0;
                    while (mismatch < count &&
                        m_firstVerification->frameHashes[mismatch] == result.frameHashes[mismatch])
                        ++mismatch;
                    result.frame = static_cast<int>(mismatch + 1);
                    result.error = "trace hash mismatch";
                }
                recordResult(pass ? "PASS" : "FAIL", result);
                ++m_index;
                m_macro.clear();
                m_firstVerification.reset();
                releaseLevel();
            }
        } catch (std::exception const& error) {
            VerificationResult result;
            result.error = error.what();
            recordResult("ERROR", result);
            m_search.reset();
            m_verification.reset();
            releaseLevel();
            ++m_index;
        }
    }

public:
    static AcceptanceRunnerNode* create() {
        auto node = new AcceptanceRunnerNode();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    ~AcceptanceRunnerNode() override {
        m_stop = true;
        m_search.reset();
        m_verification.reset();
        releaseLevel();
    }

    bool init() override {
        if (!CCLayerColor::initWithColor({0, 0, 0, 180}))
            return false;
        const auto filter = Mod::get()->getSettingValue<std::string>(
            "acceptance-filter");
        for (auto const& entry : std::filesystem::recursive_directory_iterator(
                 Mod::get()->getResourcesDir())) {
            if (!entry.is_regular_file() || entry.path().extension() != ".lvl")
                continue;
            const auto name = entry.path().stem().string();
            if (filter.empty() || name.find(filter) != std::string::npos)
                m_fixtures.push_back(entry.path());
        }
        std::sort(m_fixtures.begin(), m_fixtures.end());
        m_json << "{\"tests\":[";
        setKeypadEnabled(true);
        Build<CCLabelBMFont>::create("Preparing acceptance tests...", "bigFont.fnt")
            .id("status").scale(.5f).parent(this);
        Build<ButtonSprite>::create("Stop", "bigFont.fnt", "GJ_button_06.png")
            .intoMenuItem([this](auto*) { m_stop = true; })
            .move(0, -45).intoNewParent(CCMenu::create()).parent(this);
        schedule(schedule_selector(AcceptanceRunnerNode::step));
        return true;
    }

    void keyBackClicked() override { m_stop = true; }
};

class $modify(EditLevelLayer) {
    bool init(GJGameLevel* p0) {
        EditLevelLayer::init(p0);

        auto btn = Build<BasedButtonSprite>::create(
            CCSprite::create("pathfinder.png"_spr),
            BaseType::Circle,
            4,
            3
        ).scale(0.8);

        btn->setTopRelativeScale(1.4);

        btn.intoMenuItem([this]() {
                auto lvlString = ZipUtils::decompressString(m_level->m_levelString, true, 0);
                Build<PathfinderNode>::create(m_level, m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .intoNewParent(CCMenu::create())
          .parent(this)
          .id("pathfinder-menu")
          .matchPos(getChildByIDRecursive("delete-button"))
          .move(-45, 0);

        Build<ButtonSprite>::create("Tests", "bigFont.fnt", "GJ_button_05.png")
            .scale(.45f)
            .intoMenuItem([this](auto*) {
                Build<AcceptanceRunnerNode>::create().parent(this).zOrder(200);
            })
            .move(0, -42)
            .parent(getChildByID("pathfinder-menu"));

        return true;
    }
};

class $modify(LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        LevelInfoLayer::init(level, challenge);

        auto btn = Build<BasedButtonSprite>::create(
            CCSprite::create("pathfinder.png"_spr),
            BaseType::Circle,
            4,
            3
        ).scale(0.8);

        btn->setTopRelativeScale(1.4);

        btn.intoMenuItem([this]() {
                auto lvlString = ZipUtils::decompressString(m_level->m_levelString, true, 0);
                Build<PathfinderNode>::create(m_level, m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .parent(getChildByID("other-menu"))
          .matchPos(getChildByIDRecursive("list-button"))
          .move(0, 45);

        return true;
    }
};
