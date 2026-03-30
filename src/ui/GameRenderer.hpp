#pragma once
#include "core/GameState.hpp"
#include <SFML/Graphics.hpp>

namespace poker {

class GameRenderer {
public:
    GameRenderer(sf::RenderWindow& window, sf::Font& font);

    // Draws one frame from a state snapshot.
    // snapshot is a value copy — safe to read with no locks held.
    void render(const GameState& snapshot, PlayerId humanId);

private:
    void drawTable();
    void drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos, bool isHuman);
    void drawCard(const Card& card, sf::Vector2f pos);
    void drawCommunityCards(const GameState& state);
    void drawPot(const GameState& state);
    void drawStreetLabel(const GameState& state);
    void drawShowdownHands(const GameState& state);

    sf::RenderWindow& m_window;
    sf::Font&         m_font;
};

} // namespace poker
