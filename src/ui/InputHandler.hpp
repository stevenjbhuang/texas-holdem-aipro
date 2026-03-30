#pragma once
#include "players/HumanPlayer.hpp"
#include "core/GameState.hpp"
#include <SFML/Graphics.hpp>

namespace poker {

class InputHandler {
public:
    InputHandler(sf::RenderWindow& window, sf::Font& font, HumanPlayer& player);

    // Call once per frame — draws action buttons when it is the human's turn.
    void drawButtons(const GameState& state);

    // Pass every sf::Event from the main loop here.
    void handleEvent(const sf::Event& event, const GameState& state);

private:
    sf::RenderWindow& m_window;
    sf::Font&         m_font;
    HumanPlayer&      m_humanPlayer;

    sf::RectangleShape m_foldBtn;
    sf::RectangleShape m_callBtn;
    sf::RectangleShape m_raiseBtn;

    // Raise amount stepper
    sf::RectangleShape m_raiseMinusBtn;
    sf::RectangleShape m_raisePlusBtn;
    int m_raiseAmount = 0;   // total bet amount to submit; 0 = uninitialized
};

} // namespace poker
