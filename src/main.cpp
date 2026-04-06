#include "ui/SetupScreen.hpp"
#include "ui/GameRenderer.hpp"
#include "ui/InputHandler.hpp"
#include "ui/LetterboxView.hpp"
#include "ui/AnimationManager.hpp"
#include "ui/SoundManager.hpp"
#include "core/GameEngine.hpp"
#include "players/HumanPlayer.hpp"
#include "players/AIPlayer.hpp"
#include "ai/OpenAICompatibleClient.hpp"
#include "ai/PromptBuilder.hpp"
#include <SFML/Graphics.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>
#include <cstdlib>

#ifndef NDEBUG
#include "ui/LLMDebugLog.hpp"
#include "ui/DebugPanel.hpp"
#endif

// ---------------------------------------------------------------------------
// Helper: read entire file into string; returns "" on failure.
// ---------------------------------------------------------------------------
static std::string readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

int main()
{
    // ── Load config ──────────────────────────────────────────────────────────
    poker::LLMConfig llmConfig;
    bool debugPanel = false;

    try {
        YAML::Node cfg = YAML::LoadFile("config/game.yaml");
        std::string backendFile = cfg["llm"]["backend"].as<std::string>();

#ifndef NDEBUG
        if (cfg["debug"] && cfg["debug"]["panel"])
            debugPanel = cfg["debug"]["panel"].as<bool>();
#endif

        YAML::Node backend = YAML::LoadFile(backendFile);
        if (backend["model"])               llmConfig.model             = backend["model"].as<std::string>();
        if (backend["endpoint"])            llmConfig.endpoint          = backend["endpoint"].as<std::string>();
        if (backend["api_key"])             llmConfig.apiKey            = backend["api_key"].as<std::string>();
        if (backend["connection_timeout"])  llmConfig.connectionTimeout = backend["connection_timeout"].as<int>();
        if (backend["read_timeout"])        llmConfig.readTimeout       = backend["read_timeout"].as<int>();
        if (backend["max_tokens"])          llmConfig.maxTokens         = backend["max_tokens"].as<int>();
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not load LLM config (" << e.what()
                  << ") — using defaults.\n";
    }

    // API key: yaml value takes precedence; fall back to environment variable.
    if (llmConfig.apiKey.empty()) {
        if (const char* envKey = std::getenv("OPENAI_API_KEY")) {
            llmConfig.apiKey = envKey;
        }
    }

    // Validate required fields before attempting a connection.
    if (llmConfig.model.empty() || llmConfig.endpoint.empty()) {
        std::cerr << "Error: LLM config is missing 'model' or 'endpoint'.\n";
        return 1;
    }

    {
        poker::OpenAICompatibleClient probe(llmConfig);
        if (auto err = probe.checkReady(); !err.empty()) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }
        std::cout << "LLM ready: " << llmConfig.model << " @ " << llmConfig.endpoint << "\n";
    }

    // ── Window ───────────────────────────────────────────────────────────────
#ifndef NDEBUG
    const unsigned windowW = debugPanel
        ? static_cast<unsigned>(poker::LOGICAL_W + poker::DebugPanel::WIDTH)
        : static_cast<unsigned>(poker::LOGICAL_W);
#else
    const unsigned windowW = static_cast<unsigned>(poker::LOGICAL_W);
#endif
    constexpr unsigned windowH = static_cast<unsigned>(poker::LOGICAL_H);

    sf::RenderWindow window(
        sf::VideoMode(windowW, windowH),
        "Texas Hold'em AI Pro",
        sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize
    );
    window.setFramerateLimit(60);

#ifndef NDEBUG
    if (debugPanel) {
        float gameAreaW = static_cast<float>(windowW) - poker::DebugPanel::WIDTH;
        window.setView(poker::computeLetterboxView(window.getSize(), gameAreaW));
    } else {
        window.setView(poker::computeLetterboxView(window.getSize()));
    }
#else
    window.setView(poker::computeLetterboxView(window.getSize()));
#endif

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/DejaVuSans.ttf")) {
        std::cerr << "Could not load font — assets/fonts/DejaVuSans.ttf missing\n";
        return 1;
    }

    // ── Phase 7: setup screen ────────────────────────────────────────────────
    poker::SetupScreen       setup(window, font);
    poker::GameConfig        config    = setup.run();
    std::vector<std::string> persFiles = setup.getPersonalities();

    // SetupScreen may reset the view (it handles Resized events internally
    // without knowing about the debug panel). Re-apply the correct view now.
#ifndef NDEBUG
    if (debugPanel) {
        float gameAreaW = static_cast<float>(window.getSize().x) - poker::DebugPanel::WIDTH;
        window.setView(poker::computeLetterboxView(window.getSize(), gameAreaW));
    }
#endif

    // ── Build players ────────────────────────────────────────────────────────
    // PlayerId 0 = human; 1..n = AI.
    const poker::PlayerId humanId = 0;

#ifndef NDEBUG
    poker::LLMDebugLog debugLog;
    (void)debugLog; // suppress unused warning when panel is off
#endif

    // LLM clients must outlive AIPlayers (AIPlayer stores ILLMClient&).
    std::vector<std::unique_ptr<poker::OpenAICompatibleClient>> llmClients;
    std::vector<std::unique_ptr<poker::IPlayer>>                players;

    auto humanPlayerOwned = std::make_unique<poker::HumanPlayer>(humanId, "You");
    poker::HumanPlayer& humanPlayer = *humanPlayerOwned;
    players.push_back(std::move(humanPlayerOwned));

    for (int i = 1; i < config.numPlayers; ++i) {
        std::string persText = poker::PromptBuilder::parseSystemPrompt(readFile(persFiles[i - 1]));
        if (persText.empty())
            std::cerr << "Warning: could not load personality file " << persFiles[i - 1] << "\n";

        llmClients.push_back(std::make_unique<poker::OpenAICompatibleClient>(llmConfig));

#ifndef NDEBUG
        if (debugPanel) {
            llmClients.back()->onPromptComplete =
                [&debugLog](const std::string& sys, const std::string& user, const std::string& resp) {
                    debugLog.push(sys, user, resp);
                };
        }
#endif

        players.push_back(std::make_unique<poker::AIPlayer>(
            i,
            "AI " + std::to_string(i),
            *llmClients.back(),
            persText
        ));
    }

    // ── Wire engine ──────────────────────────────────────────────────────────
    poker::GameEngine engine(std::move(players), config);

    // ── Spawn game thread ────────────────────────────────────────────────────
    std::atomic<bool> running{true};
    std::thread gameThread([&]() {
        while (running && !engine.isGameOver()) {
            engine.tick();
        }
    });

    // ── UI objects ───────────────────────────────────────────────────────────
    poker::GameRenderer     renderer(window, font);
    poker::InputHandler     inputHandler(window, font, humanPlayer);
    poker::AnimationManager animMgr;
    poker::SoundManager     soundMgr;
    soundMgr.loadAll();

#ifndef NDEBUG
    std::unique_ptr<poker::DebugPanel> debugPanelUI;
    if (debugPanel) debugPanelUI = std::make_unique<poker::DebugPanel>(window, font, debugLog);
#endif

    bool      isFullscreen = false;
    sf::Clock frameClock;

    // ── Main render / event loop ─────────────────────────────────────────────
    while (window.isOpen()) {
        sf::Time dt = frameClock.restart();

        // 1. Get a fresh snapshot once per frame (mutex-protected copy).
        poker::GameState snapshot = engine.getStateSnapshot();

        // 2. Detect state changes and advance animations.
        renderer.detectDelta(snapshot, animMgr, soundMgr);
        animMgr.update(dt);

        // 3. Process events.
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::Resized) {
#ifndef NDEBUG
                if (debugPanel) {
                    float gameAreaW = static_cast<float>(event.size.width) - poker::DebugPanel::WIDTH;
                    window.setView(poker::computeLetterboxView({event.size.width, event.size.height}, gameAreaW));
                    debugPanelUI->onResize(event.size.width, event.size.height);
                } else {
                    window.setView(poker::computeLetterboxView({event.size.width, event.size.height}));
                }
#else
                window.setView(poker::computeLetterboxView({event.size.width, event.size.height}));
#endif
            }
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::F11) {
#ifndef NDEBUG
                // F11 fullscreen not supported when the debug panel is visible.
                if (!debugPanel)
#endif
                {
                    isFullscreen = !isFullscreen;
                    if (isFullscreen) {
                        window.create(sf::VideoMode::getDesktopMode(),
                                      "Texas Hold'em AI Pro",
                                      sf::Style::Fullscreen);
                    } else {
                        window.create(sf::VideoMode(static_cast<unsigned>(poker::LOGICAL_W),
                                                   static_cast<unsigned>(poker::LOGICAL_H)),
                                      "Texas Hold'em AI Pro",
                                      sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize);
                    }
                    window.setFramerateLimit(60);
                    window.setView(poker::computeLetterboxView(window.getSize()));
                    renderer.reloadAssets();
                }
            }
#ifndef NDEBUG
            if (debugPanelUI && event.type == sf::Event::MouseWheelScrolled) {
                debugPanelUI->scroll(-event.mouseWheelScroll.delta * 30.f);
            } else
#endif
            inputHandler.handleEvent(event, snapshot);
        }

        // 4. Draw.
        renderer.render(snapshot, humanId, animMgr);
        inputHandler.drawButtons(snapshot);
#ifndef NDEBUG
        if (debugPanelUI) debugPanelUI->draw();
#endif
        window.display();
    }

    // ── Clean shutdown ───────────────────────────────────────────────────────
    running = false;
    for (auto& client : llmClients) {
        client->stop();
    }
    if (humanPlayer.isWaitingForInput()) {
        humanPlayer.provideAction(poker::Action{poker::Action::Type::Fold});
    }
    gameThread.join();

    return 0;
}
