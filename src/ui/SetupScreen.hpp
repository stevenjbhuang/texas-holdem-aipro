#pragma once
#include "core/GameEngine.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace poker {

class SetupScreen {
public:
    SetupScreen(sf::RenderWindow& window, sf::Font& font);

    // Blocks and processes events until the user clicks Start.
    // Returns the configured GameConfig.
    GameConfig run();

    // Returns per-AI-player personality file paths in player order (AI 1..N).
    // Valid only after run() returns.
    std::vector<std::string> getPersonalities() const;

private:
    void draw();

    // Personality helpers
    void scanPersonalities();          // populates m_persFiles / m_persNames
    float personalityRowY(int i) const;  // y-coordinate for AI player i's row

    sf::RenderWindow& m_window;
    sf::Font&         m_font;

    int m_numAIPlayers  = 3;
    int m_startingStack = 1000;
    int m_smallBlind    = 5;
    int m_bigBlind      = 10;

    sf::RectangleShape m_minusAI;
    sf::RectangleShape m_plusAI;
    sf::RectangleShape m_minusStack;
    sf::RectangleShape m_plusStack;
    sf::RectangleShape m_startBtn;

    // Available personalities discovered from config/personalities/
    std::vector<std::string> m_persFiles;   // e.g. "config/personalities/aggressive.md"
    std::vector<std::string> m_persNames;   // display name, e.g. "Aggressive"

    // Per-AI-player selected personality index (into m_persFiles).
    // Resized whenever m_numAIPlayers changes.
    std::vector<int> m_persIdx;

    // Per-AI-player [<] and [>] cycle buttons (max 6 rows).
    static constexpr int MAX_AI = 6;
    sf::RectangleShape m_persLeft[MAX_AI];
    sf::RectangleShape m_persRight[MAX_AI];
};

}