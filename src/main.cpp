#include "ui/SetupScreen.hpp"
#include "ui/GameRenderer.hpp"
#include "ui/InputHandler.hpp"
#include "ui/LetterboxView.hpp"
#include "ui/AnimationManager.hpp"
#include "ui/SoundManager.hpp"
#include "core/GameEngine.hpp"
#include "players/HumanPlayer.hpp"
#include "players/AIPlayer.hpp"
#include "ai/OllamaClient.hpp"
#include <SFML/Graphics.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <atomic>
#include <thread>
#include <iostream>

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
    // ── Load LLM config and check Ollama before opening any window ───────────
    std::string llmModel    = "llama3.2";
    std::string llmEndpoint = "http://localhost:11434";
    int  connTimeout = 5;
    int  readTimeout = 30;
    bool llmThink    = true;

    try {
        YAML::Node cfg = YAML::LoadFile("config/game.yaml");
        auto llm = cfg["llm"];
        if (llm["model"])               llmModel    = llm["model"].as<std::string>();
        if (llm["endpoint"])            llmEndpoint = llm["endpoint"].as<std::string>();
        if (llm["connection_timeout"])  connTimeout = llm["connection_timeout"].as<int>();
        if (llm["read_timeout"])        readTimeout = llm["read_timeout"].as<int>();
        if (llm["think"])               llmThink    = llm["think"].as<bool>();
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not load config/game.yaml (" << e.what()
                  << ") — using defaults.\n";
    }

    {
        poker::OllamaClient probe(llmModel, llmEndpoint, connTimeout, readTimeout);
        if (!probe.isServerAvailable()) {
            std::cerr << "Error: Ollama server not reachable at " << llmEndpoint
                      << "\n  Start it with: ollama serve\n";
            return 1;
        }
        if (!probe.isModelAvailable()) {
            std::cerr << "Error: model '" << llmModel << "' not found in Ollama."
                      << "\n  Pull it with: ollama pull " << llmModel << "\n";
            return 1;
        }
        std::cout << "LLM ready: " << llmModel << " @ " << llmEndpoint << "\n";
    }

    sf::RenderWindow window(
        sf::VideoMode(static_cast<unsigned>(poker::LOGICAL_W),
                      static_cast<unsigned>(poker::LOGICAL_H)),
        "Texas Hold'em AI Pro",
        sf::Style::Titlebar | sf::Style::Close | sf::Style::Resize
    );
    window.setFramerateLimit(60);
    // Letterbox view is applied immediately so every screen — including setup —
    // renders in the 800×700 logical canvas with correct aspect ratio.
    window.setView(poker::computeLetterboxView(window.getSize()));

    sf::Font font;
    if (!font.loadFromFile("assets/fonts/DejaVuSans.ttf")) {
        std::cerr << "Could not load font — assets/fonts/DejaVuSans.ttf missing\n";
        return 1;
    }

    // ── Phase 7: setup screen ────────────────────────────────────────────────
    poker::SetupScreen       setup(window, font);
    poker::GameConfig        config    = setup.run();
    std::vector<std::string> persFiles = setup.getPersonalities();

    // ── Build players ────────────────────────────────────────────────────────
    // PlayerId 0 = human; 1..n = AI.
    const poker::PlayerId humanId = 0;

    // OllamaClients must outlive AIPlayers (AIPlayer stores ILLMClient&).
    std::vector<std::unique_ptr<poker::OllamaClient>> ollamaClients;
    std::vector<std::unique_ptr<poker::IPlayer>>      players;

    auto humanPlayerOwned = std::make_unique<poker::HumanPlayer>(humanId, "You");
    poker::HumanPlayer& humanPlayer = *humanPlayerOwned;  // keep ref for InputHandler + shutdown
    players.push_back(std::move(humanPlayerOwned));

    for (int i = 1; i < config.numPlayers; ++i) {
        // persFiles is indexed 0..numAIPlayers-1; AI players are 1..numPlayers-1.
        std::string persText = readFile(persFiles[i - 1]);
        if (persText.empty())
            std::cerr << "Warning: could not load personality file " << persFiles[i - 1] << "\n";

        ollamaClients.push_back(
            std::make_unique<poker::OllamaClient>(llmModel, llmEndpoint, connTimeout, readTimeout, llmThink)
        );
        players.push_back(std::make_unique<poker::AIPlayer>(
            i,
            "AI " + std::to_string(i),
            *ollamaClients.back(),
            persText
        ));
    }

    // ── Wire engine ──────────────────────────────────────────────────────────
    poker::GameEngine engine(std::move(players), config);

    // ── Spawn game thread ────────────────────────────────────────────────────
    // tick() blocks on each player's action; the main thread must stay
    // responsive so SFML can deliver input via InputHandler::provideAction().
    std::atomic<bool> running{true};
    std::thread gameThread([&]() {
        while (running && !engine.isGameOver()) {
            engine.tick();
        }
    });

    // ── UI objects ───────────────────────────────────────────────────────────
    poker::GameRenderer    renderer(window, font);
    poker::InputHandler    inputHandler(window, font, humanPlayer);
    poker::AnimationManager animMgr;
    poker::SoundManager    soundMgr;
    soundMgr.loadAll();

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
            // Recompute the letterbox view whenever the window is resized.
            if (event.type == sf::Event::Resized) {
                window.setView(poker::computeLetterboxView({event.size.width, event.size.height}));
            }
            // F11 toggles fullscreen. window.create() reinitialises in-place so
            // existing sf::RenderWindow& references (renderer, inputHandler) stay valid.
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::F11) {
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
                renderer.reloadAssets();  // window.create() destroys the OpenGL context
            }
            inputHandler.handleEvent(event, snapshot);
        }

        // 4. Draw.
        renderer.render(snapshot, humanId, animMgr);
        inputHandler.drawButtons(snapshot);
        window.display();
    }

    // ── Clean shutdown ───────────────────────────────────────────────────────
    // Stop the game thread, then unblock any blocking getAction() call so
    // the thread can observe the running=false flag and exit cleanly.
    running = false;
    // Cancel any in-flight LLM HTTP request so the game thread isn't stuck
    // waiting for a response that will never be acted on.
    for (auto& client : ollamaClients) {
        client->stop();
    }
    if (humanPlayer.isWaitingForInput()) {
        humanPlayer.provideAction(poker::Action{poker::Action::Type::Fold});
    }
    gameThread.join();

    return 0;
}
