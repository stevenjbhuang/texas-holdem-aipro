#include "ui/InputHandler.hpp"
#include <algorithm>

namespace poker {

InputHandler::InputHandler(sf::RenderWindow& window, sf::Font& font, HumanPlayer& player)
    : m_window(window), m_font(font), m_humanPlayer(player)
{
    // Three action buttons — moved down to y=625 to make room for the raise stepper above.
    // Total width: 110 + 20 + 130 + 20 + 130 = 410px  →  start x = (800-410)/2 = 195
    m_foldBtn.setSize({110.f, 42.f});
    m_foldBtn.setPosition(195.f, 625.f);

    m_callBtn.setSize({130.f, 42.f});
    m_callBtn.setPosition(325.f, 625.f);

    m_raiseBtn.setSize({130.f, 42.f});
    m_raiseBtn.setPosition(475.f, 625.f);

    // Raise amount stepper — flanks a label centred above the raise button.
    // Raise button: x=475..605, centre=540. Stepper spans x=455..615.
    m_raiseMinusBtn.setSize({28.f, 26.f});
    m_raiseMinusBtn.setPosition(455.f, 594.f);
    m_raiseMinusBtn.setFillColor(sf::Color(90, 60, 60));

    m_raisePlusBtn.setSize({28.f, 26.f});
    m_raisePlusBtn.setPosition(587.f, 594.f);
    m_raisePlusBtn.setFillColor(sf::Color(60, 90, 60));
}

void InputHandler::drawButtons(const GameState& state)
{
    // Guard: only draw when the engine is blocked waiting for this human.
    if (!m_humanPlayer.isWaitingForInput()) return;

    PlayerId id = state.activePlayer;

    int maxBet  = 0;
    for (auto& [pid, bet] : state.currentBets)
        maxBet = std::max(maxBet, bet);

    int myBet    = state.currentBets.count(id) ? state.currentBets.at(id) : 0;
    int myChips  = state.chipCounts.count(id)  ? state.chipCounts.at(id)  : 0;
    int callCost = maxBet - myBet;
    int minRaise = maxBet + state.minRaise;
    int maxRaise = myChips + myBet;   // all-in total
    bool canRaise = minRaise <= maxRaise;

    // Keep m_raiseAmount in [minRaise, maxRaise]; re-initialise if stale.
    if (canRaise) {
        if (m_raiseAmount < minRaise || m_raiseAmount > maxRaise)
            m_raiseAmount = minRaise;
    }

    auto drawBtn = [&](sf::RectangleShape& btn, const std::string& label, sf::Color col) {
        btn.setFillColor(col);
        m_window.draw(btn);

        sf::Text t(label, m_font, 16);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition(
            btn.getPosition().x + (btn.getSize().x - tb.width)  / 2.f,
            btn.getPosition().y + (btn.getSize().y - tb.height) / 2.f - 2.f
        );
        t.setFillColor(sf::Color::White);
        m_window.draw(t);
    };

    drawBtn(m_foldBtn,  "Fold",  sf::Color(160, 50, 50));
    drawBtn(m_callBtn,  callCost == 0 ? "Check" : "Call  $" + std::to_string(callCost),
            sf::Color(50, 100, 180));
    drawBtn(m_raiseBtn, canRaise ? "Raise" : "All-in", sf::Color(160, 130, 30));

    // Raise amount stepper — only draw when a legal raise exists.
    if (!canRaise) return;

    auto drawSmall = [&](sf::RectangleShape& btn, const std::string& label) {
        m_window.draw(btn);
        sf::Text t(label, m_font, 15);
        sf::FloatRect tb = t.getLocalBounds();
        t.setPosition(
            btn.getPosition().x + (btn.getSize().x - tb.width)  / 2.f,
            btn.getPosition().y + (btn.getSize().y - tb.height) / 2.f - 2.f
        );
        t.setFillColor(sf::Color::White);
        m_window.draw(t);
    };

    drawSmall(m_raiseMinusBtn, "-");
    drawSmall(m_raisePlusBtn,  "+");

    // Amount label centred between the two stepper buttons.
    std::string amtStr = (m_raiseAmount >= maxRaise) ? "All-in $" + std::to_string(m_raiseAmount)
                                                      : "$" + std::to_string(m_raiseAmount);
    sf::Text amtText(amtStr, m_font, 14);
    sf::FloatRect ab = amtText.getLocalBounds();
    amtText.setPosition(540.f - ab.width / 2.f, 599.f);
    amtText.setFillColor(sf::Color(255, 220, 80));
    m_window.draw(amtText);
}

void InputHandler::handleEvent(const sf::Event& event, const GameState& state)
{
    // Guard: ignore events when it is not the human's turn.
    if (!m_humanPlayer.isWaitingForInput()) return;

    // Compute raise bounds (needed for stepper and scroll).
    PlayerId id  = state.activePlayer;
    int maxBet   = 0;
    for (auto& [pid, bet] : state.currentBets)
        maxBet = std::max(maxBet, bet);
    int myBet    = state.currentBets.count(id) ? state.currentBets.at(id) : 0;
    int myChips  = state.chipCounts.count(id)  ? state.chipCounts.at(id)  : 0;
    int minRaise = maxBet + state.minRaise;
    int maxRaise = myChips + myBet;
    int step     = state.minRaise;

    // Mouse wheel over the raise area adjusts the amount.
    if (event.type == sf::Event::MouseWheelScrolled) {
        sf::FloatRect raiseArea(455.f, 594.f, 160.f, 73.f);  // covers stepper + raise button
        // Convert pixel coords → logical 800×700 space (accounts for resize/fullscreen).
        sf::Vector2f logical = m_window.mapPixelToCoords(
            {static_cast<int>(event.mouseWheelScroll.x),
             static_cast<int>(event.mouseWheelScroll.y)});
        float mx = logical.x;
        float my = logical.y;
        if (raiseArea.contains(mx, my) && minRaise <= maxRaise) {
            m_raiseAmount += static_cast<int>(event.mouseWheelScroll.delta) * step;
            m_raiseAmount = std::max(minRaise, std::min(maxRaise, m_raiseAmount));
        }
        return;
    }

    if (event.type != sf::Event::MouseButtonPressed) return;
    if (event.mouseButton.button != sf::Mouse::Left)  return;

    // Convert pixel coords → logical 800×700 space (accounts for resize/fullscreen).
    sf::Vector2f logical = m_window.mapPixelToCoords(
        {event.mouseButton.x, event.mouseButton.y});
    float mx = logical.x;
    float my = logical.y;

    if (m_foldBtn.getGlobalBounds().contains(mx, my)) {
        m_raiseAmount = 0;
        m_humanPlayer.provideAction(Action{Action::Type::Fold});
        return;
    }

    if (m_callBtn.getGlobalBounds().contains(mx, my)) {
        m_raiseAmount = 0;
        m_humanPlayer.provideAction(Action{Action::Type::Call});
        return;
    }

    // Stepper: adjust raise amount without committing.
    if (m_raiseMinusBtn.getGlobalBounds().contains(mx, my) && minRaise <= maxRaise) {
        m_raiseAmount = std::max(minRaise, m_raiseAmount - step);
        return;
    }

    if (m_raisePlusBtn.getGlobalBounds().contains(mx, my) && minRaise <= maxRaise) {
        m_raiseAmount = std::min(maxRaise, m_raiseAmount + step);
        return;
    }

    // Raise button commits m_raiseAmount.
    if (m_raiseBtn.getGlobalBounds().contains(mx, my) && minRaise <= maxRaise) {
        int amount = (m_raiseAmount >= minRaise) ? m_raiseAmount : minRaise;
        m_raiseAmount = 0;
        m_humanPlayer.provideAction(Action{Action::Type::Raise, amount});
    }
}

} // namespace poker
