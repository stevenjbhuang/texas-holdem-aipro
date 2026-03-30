#include "ui/SetupScreen.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace poker {

// ── Personality discovery ─────────────────────────────────────────────────────

void SetupScreen::scanPersonalities()
{
    namespace fs = std::filesystem;
    const std::string dir = "config/personalities";

    try {
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".md") {
                m_persFiles.push_back(entry.path().string());
                std::string name = entry.path().stem().string();
                if (!name.empty()) name[0] = static_cast<char>(std::toupper(name[0]));
                m_persNames.push_back(name);
            }
        }
    } catch (...) {}

    // Fallback so the UI always has at least one option.
    if (m_persFiles.empty()) {
        m_persFiles.push_back("config/personalities/default.md");
        m_persNames.push_back("Default");
    }

    // Sort alphabetically by display name for consistency across platforms.
    std::vector<int> order(m_persFiles.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(),
              [&](int a, int b){ return m_persNames[a] < m_persNames[b]; });

    std::vector<std::string> sortedFiles, sortedNames;
    for (int i : order) {
        sortedFiles.push_back(m_persFiles[i]);
        sortedNames.push_back(m_persNames[i]);
    }
    m_persFiles = std::move(sortedFiles);
    m_persNames = std::move(sortedNames);
}

float SetupScreen::personalityRowY(int i) const
{
    return 402.f + static_cast<float>(i) * 36.f;
}

// ── Construction ──────────────────────────────────────────────────────────────

SetupScreen::SetupScreen(sf::RenderWindow& window, sf::Font& font)
    : m_window(window), m_font(font)
{
    auto initSmall = [](sf::RectangleShape& btn, float x, float y) {
        btn.setSize({30.f, 30.f});
        btn.setPosition(x, y);
        btn.setFillColor(sf::Color(80, 80, 80));
    };

    // AI player +/- buttons at y=200
    initSmall(m_minusAI,    280.f, 200.f);
    initSmall(m_plusAI,     450.f, 200.f);

    // Starting stack +/- buttons at y=260
    initSmall(m_minusStack, 280.f, 260.f);
    initSmall(m_plusStack,  450.f, 260.f);

    // Start button — position updated dynamically in draw()
    m_startBtn.setSize({160.f, 48.f});
    m_startBtn.setFillColor(sf::Color(40, 140, 40));

    // Personality cycle buttons: sized here, positioned in draw()
    for (int i = 0; i < MAX_AI; ++i) {
        m_persLeft[i].setSize({26.f, 26.f});
        m_persLeft[i].setFillColor(sf::Color(70, 70, 90));
        m_persRight[i].setSize({26.f, 26.f});
        m_persRight[i].setFillColor(sf::Color(70, 70, 90));
    }

    scanPersonalities();

    // Default all AI players to the first personality.
    m_persIdx.assign(m_numAIPlayers, 0);
}

// ── Public interface ──────────────────────────────────────────────────────────

std::vector<std::string> SetupScreen::getPersonalities() const
{
    std::vector<std::string> result;
    for (int i = 0; i < m_numAIPlayers; ++i) {
        int idx = (i < static_cast<int>(m_persIdx.size())) ? m_persIdx[i] : 0;
        result.push_back(m_persFiles[idx]);
    }
    return result;
}

GameConfig SetupScreen::run()
{
    bool done = false;

    while (m_window.isOpen() && !done) {
        sf::Event event;
        while (m_window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                m_window.close();
                return GameConfig(2, m_startingStack, m_smallBlind, m_bigBlind);
            }

            if (event.type == sf::Event::MouseButtonPressed &&
                event.mouseButton.button == sf::Mouse::Left)
            {
                float mx = static_cast<float>(event.mouseButton.x);
                float my = static_cast<float>(event.mouseButton.y);

                if (m_minusAI.getGlobalBounds().contains(mx, my)) {
                    m_numAIPlayers = std::max(1, m_numAIPlayers - 1);
                    m_persIdx.resize(m_numAIPlayers, 0);
                }
                else if (m_plusAI.getGlobalBounds().contains(mx, my)) {
                    m_numAIPlayers = std::min(MAX_AI, m_numAIPlayers + 1);
                    m_persIdx.resize(m_numAIPlayers, 0);
                }
                else if (m_minusStack.getGlobalBounds().contains(mx, my))
                    m_startingStack = std::max(200, m_startingStack - 100);
                else if (m_plusStack.getGlobalBounds().contains(mx, my))
                    m_startingStack = std::min(10000, m_startingStack + 100);
                else if (m_startBtn.getGlobalBounds().contains(mx, my))
                    done = true;
                else {
                    // Personality cycle buttons for each AI player row
                    int n = static_cast<int>(m_persFiles.size());
                    for (int i = 0; i < m_numAIPlayers; ++i) {
                        if (m_persLeft[i].getGlobalBounds().contains(mx, my)) {
                            m_persIdx[i] = (m_persIdx[i] - 1 + n) % n;
                            break;
                        }
                        if (m_persRight[i].getGlobalBounds().contains(mx, my)) {
                            m_persIdx[i] = (m_persIdx[i] + 1) % n;
                            break;
                        }
                    }
                }
            }
        }

        draw();
        m_window.display();
    }

    return GameConfig(m_numAIPlayers + 1, m_startingStack, m_smallBlind, m_bigBlind);
}

// ── Drawing ───────────────────────────────────────────────────────────────────

void SetupScreen::draw()
{
    m_window.clear(sf::Color(25, 25, 35));

    auto makeText = [&](const std::string& str, float x, float y,
                        unsigned size = 22, sf::Color col = sf::Color::White)
    {
        sf::Text t(str, m_font, size);
        t.setPosition(x, y);
        t.setFillColor(col);
        return t;
    };

    // ── Title ────────────────────────────────────────────────────
    m_window.draw(makeText("Texas Hold'em AI Pro", 190.f, 50.f, 36));
    m_window.draw(makeText("Game Setup", 310.f, 102.f, 20, sf::Color(170, 170, 170)));

    // ── AI Players row ───────────────────────────────────────────
    m_window.draw(makeText("AI Players:", 80.f, 208.f));
    m_window.draw(m_minusAI);
    m_window.draw(makeText("-", 289.f, 209.f, 20));
    m_window.draw(makeText(std::to_string(m_numAIPlayers), 355.f, 207.f, 24,
                            sf::Color::Yellow));
    m_window.draw(m_plusAI);
    m_window.draw(makeText("+", 459.f, 209.f, 20));

    // ── Starting Stack row ───────────────────────────────────────
    m_window.draw(makeText("Starting Stack:", 80.f, 268.f));
    m_window.draw(m_minusStack);
    m_window.draw(makeText("-", 289.f, 269.f, 20));
    m_window.draw(makeText("$" + std::to_string(m_startingStack), 330.f, 267.f, 24,
                            sf::Color::Yellow));
    m_window.draw(m_plusStack);
    m_window.draw(makeText("+", 459.f, 269.f, 20));

    // ── Blinds (fixed) ───────────────────────────────────────────
    m_window.draw(makeText("Blinds:  $" + std::to_string(m_smallBlind) +
                            " / $" + std::to_string(m_bigBlind),
                            80.f, 320.f, 20, sf::Color(160, 160, 160)));

    // ── Personality section ──────────────────────────────────────
    m_window.draw(makeText("AI Personalities:", 80.f, 365.f, 18,
                            sf::Color(200, 200, 200)));

    for (int i = 0; i < m_numAIPlayers; ++i) {
        float ry = personalityRowY(i);

        // Update cycle button positions (used for hit-testing in run())
        m_persLeft[i].setPosition(228.f, ry);
        m_persRight[i].setPosition(374.f, ry);

        // "AI N:" label
        m_window.draw(makeText("AI " + std::to_string(i + 1) + ":", 100.f, ry + 3.f, 16));

        // [<] button
        m_window.draw(m_persLeft[i]);
        m_window.draw(makeText("<", 234.f, ry + 4.f, 14));

        // Personality name centred between the two buttons (x=257..370, centre=313)
        const std::string& name = m_persNames[m_persIdx[i]];
        sf::Text nameText(name, m_font, 15);
        sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setPosition(313.f - nb.width / 2.f, ry + 5.f);
        nameText.setFillColor(sf::Color::Yellow);
        m_window.draw(nameText);

        // [>] button
        m_window.draw(m_persRight[i]);
        m_window.draw(makeText(">", 380.f, ry + 4.f, 14));
    }

    // ── Start button — position is dynamic based on number of AI players ──────
    float startY = std::max(455.f, personalityRowY(m_numAIPlayers) + 15.f);
    m_startBtn.setPosition(320.f, startY);

    m_window.draw(m_startBtn);

    // Centre "Start Game" inside the button
    sf::Text startLabel("Start Game", m_font, 22);
    sf::FloatRect slb = startLabel.getLocalBounds();
    startLabel.setPosition(320.f + (160.f - slb.width) / 2.f, startY + (48.f - slb.height) / 2.f - 2.f);
    startLabel.setFillColor(sf::Color::White);
    m_window.draw(startLabel);
}

} // namespace poker
