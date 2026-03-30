#include "ui/GameRenderer.hpp"
#include <algorithm>
#include <string>

namespace poker {

// ── Layout constants ──────────────────────────────────────────────────────────
//
//  Window: 800 × 700
//
//  y=0   ┌──────────────────────────────────────────────────────────────────┐
//        │ Street label (left)          "Thinking…" (right)                 │
//  y=55  ├──────────────────────────────────────────────────────────────────┤
//        │ Top seat panels   (seats 3, 4)                                   │
//  y=130 ├──────────────────────────────────────────────────────────────────┤
//        │                                                                  │
//        │  [Table felt 580×270 at (110,148)]                               │
//        │     Pot label (y=222)                                            │
//        │     Community cards (y=265)                                      │
//        │                                                                  │
//  y=420 ├──────────────────────────────────────────────────────────────────┤
//        │ Side-bottom seat panels  (seats 1, 6) at y=432                   │
//  y=450 ├──────────────────────────────────────────────────────────────────┤
//        │ Human seat panel                  at y=455                       │
//  y=510 ├──────────────────────────────────────────────────────────────────┤
//        │ Human hole cards (50×70)          at y=516                       │
//  y=590 ├──────────────────────────────────────────────────────────────────┤
//        │ Action buttons (42px tall)        at y=600                       │
//  y=645 └──────────────────────────────────────────────────────────────────┘

static const sf::Vector2f SEATS[] = {
    {335.f, 455.f},   // 0 — human        (bottom-centre)
    {620.f, 432.f},   // 1 — bottom-right
    {672.f, 248.f},   // 2 — right
    {510.f,  68.f},   // 3 — top-right
    {190.f,  68.f},   // 4 — top-left
    { 18.f, 248.f},   // 5 — left
    { 78.f, 432.f},   // 6 — bottom-left
};
static constexpr int   MAX_SEATS    = 7;
static constexpr float PANEL_W      = 130.f;
static constexpr float PANEL_H      =  52.f;
static constexpr float CARD_W       =  50.f;
static constexpr float CARD_H       =  70.f;
static constexpr float CARD_GAP     =   8.f;   // gap between adjacent cards

GameRenderer::GameRenderer(sf::RenderWindow& window, sf::Font& font)
    : m_window(window), m_font(font) {}

void GameRenderer::render(const GameState& state, PlayerId humanId)
{
    m_window.clear(sf::Color(0, 90, 0));

    drawTable();
    drawCommunityCards(state);
    drawPot(state);
    drawStreetLabel(state);
    drawShowdownHands(state);

    // Build player list with human always at index 0 (SEATS[0]).
    std::vector<PlayerId> players;
    players.push_back(humanId);
    for (auto& [id, _] : state.chipCounts)
        if (id != humanId) players.push_back(id);

    for (int i = 0; i < static_cast<int>(players.size()) && i < MAX_SEATS; ++i)
        drawPlayer(players[i], state, SEATS[i], players[i] == humanId);

    // Human's hole cards — centered below the human seat panel.
    if (state.holeCards.count(humanId)) {
        const Hand& h = state.holeCards.at(humanId);
        // Two cards centred under the panel (panel centre x = 335 + 65 = 400).
        drawCard(h.first,  {346.f, 516.f});
        drawCard(h.second, {404.f, 516.f});
    }

    // "Thinking…" — top-right, away from street label.
    if (state.waitingForAction && state.activePlayer != humanId) {
        sf::Text thinking("Thinking...", m_font, 18);
        thinking.setFillColor(sf::Color::Yellow);
        thinking.setStyle(sf::Text::Italic);
        sf::FloatRect tb = thinking.getLocalBounds();
        thinking.setPosition(800.f - tb.width - 14.f, 12.f);
        m_window.draw(thinking);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

void GameRenderer::drawTable()
{
    sf::RectangleShape felt(sf::Vector2f(580.f, 270.f));
    felt.setPosition(110.f, 148.f);
    felt.setFillColor(sf::Color(0, 110, 0));
    felt.setOutlineColor(sf::Color(101, 67, 33));
    felt.setOutlineThickness(10.f);
    m_window.draw(felt);
}

void GameRenderer::drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos, bool isHuman)
{
    bool folded = state.foldedPlayers.count(id) > 0;
    bool active = state.waitingForAction && state.activePlayer == id;
    int  chips  = state.chipCounts.count(id)  ? state.chipCounts.at(id)  : 0;
    int  bet    = state.currentBets.count(id) ? state.currentBets.at(id) : 0;
    bool busted = (chips == 0 && bet == 0);

    sf::RectangleShape panel(sf::Vector2f(PANEL_W, PANEL_H));
    panel.setPosition(pos);
    if      (active)  panel.setFillColor(sf::Color(200, 170,  20));
    else if (folded)  panel.setFillColor(sf::Color( 60,  60,  60));
    else if (busted)  panel.setFillColor(sf::Color(100,  20,  20));
    else              panel.setFillColor(sf::Color( 40,  80,  40));
    bool isWinner = (state.street == Street::Showdown && state.showdownWinners.count(id) > 0);
    panel.setOutlineColor(isWinner ? sf::Color(255, 220, 80) : sf::Color::Black);
    panel.setOutlineThickness(isWinner ? 3.f : 1.f);
    m_window.draw(panel);

    std::string statusLine = folded ? "FOLDED"
                           : busted ? "BUST"
                           : "$" + std::to_string(chips);
    if (!folded && !busted && bet > 0)
        statusLine += "  bet:$" + std::to_string(bet);

    std::string posLabel;
    if      (id == state.dealerButton)   posLabel = " [D]";
    else if (id == state.smallBlindSeat) posLabel = " [SB]";
    else if (id == state.bigBlindSeat)   posLabel = " [BB]";

    sf::Text nameText("P" + std::to_string(id) + posLabel, m_font, 14);
    nameText.setPosition(pos.x + 6.f, pos.y + 5.f);
    nameText.setFillColor(isWinner ? sf::Color(255, 240, 170) : sf::Color::White);
    m_window.draw(nameText);

    sf::Text chipsText(statusLine, m_font, 13);
    chipsText.setPosition(pos.x + 6.f, pos.y + 30.f);
    chipsText.setFillColor(folded ? sf::Color(140, 140, 140) : sf::Color::White);
    m_window.draw(chipsText);

    // Last action badge — skipped for the human player (hole cards sit immediately
    // below their panel and would overlap). For all other seats a dark background
    // rectangle is drawn first so the text is legible on any surface (felt, etc.).
    if (!isHuman && state.lastActions.count(id)) {
        const Action& a = state.lastActions.at(id);
        std::string actionStr;
        sf::Color   actionColor;
        switch (a.type) {
            case Action::Type::Fold:
                actionStr   = "Fold";
                actionColor = sf::Color(220, 80, 80);
                break;
            case Action::Type::Call:
                if (a.amount == 0) {
                    actionStr   = "Check";
                    actionColor = sf::Color(100, 200, 255);
                } else {
                    actionStr   = "Call $" + std::to_string(a.amount);
                    actionColor = sf::Color(80, 200, 120);
                }
                break;
            case Action::Type::Raise:
                actionStr   = "Raise $" + std::to_string(a.amount);
                actionColor = sf::Color(255, 210, 60);
                break;
        }

        sf::Text actionText(actionStr, m_font, 11);
        sf::FloatRect tb = actionText.getLocalBounds();
        float bx = pos.x + 2.f;
        float by = pos.y + PANEL_H + 2.f;

        // Dark backing rect so the badge is readable on the felt or any background.
        sf::RectangleShape backing(sf::Vector2f(tb.width + 8.f, tb.height + 6.f));
        backing.setPosition(bx, by);
        backing.setFillColor(sf::Color(20, 20, 20, 200));
        m_window.draw(backing);

        actionText.setFillColor(actionColor);
        actionText.setPosition(bx + 4.f, by + 2.f);
        m_window.draw(actionText);
    }
}

void GameRenderer::drawCard(const Card& card, sf::Vector2f pos)
{
    sf::RectangleShape rect(sf::Vector2f(CARD_W, CARD_H));
    rect.setPosition(pos);
    rect.setFillColor(sf::Color::White);
    rect.setOutlineColor(sf::Color(60, 60, 60));
    rect.setOutlineThickness(1.f);
    m_window.draw(rect);

    bool red = card.getSuit() == Suit::Hearts || card.getSuit() == Suit::Diamonds;
    sf::Text label(card.toShortString(), m_font, 20);
    label.setFillColor(red ? sf::Color(200, 0, 0) : sf::Color::Black);
    label.setPosition(pos.x + 6.f, pos.y + 8.f);
    m_window.draw(label);
}

void GameRenderer::drawCommunityCards(const GameState& state)
{
    // 5 cards × 50px + 4 gaps × 8px = 282px total; centre at x=400 → start at 259.
    float x = 259.f;
    for (const Card& c : state.communityCards) {
        drawCard(c, {x, 265.f});
        x += CARD_W + CARD_GAP;
    }
    // Placeholder slots for undealt cards.
    for (int i = static_cast<int>(state.communityCards.size()); i < 5; ++i) {
        sf::RectangleShape slot(sf::Vector2f(CARD_W, CARD_H));
        slot.setPosition(x, 265.f);
        slot.setFillColor(sf::Color(0, 80, 0));
        slot.setOutlineColor(sf::Color(0, 140, 0));
        slot.setOutlineThickness(1.f);
        m_window.draw(slot);
        x += CARD_W + CARD_GAP;
    }
}

void GameRenderer::drawPot(const GameState& state)
{
    if (state.pot == 0) return;
    sf::Text pot("Pot  $" + std::to_string(state.pot), m_font, 20);
    pot.setFillColor(sf::Color::White);
    sf::FloatRect bounds = pot.getLocalBounds();
    pot.setPosition(400.f - bounds.width / 2.f, 222.f);
    m_window.draw(pot);
}

void GameRenderer::drawStreetLabel(const GameState& state)
{
    const char* label = "";
    switch (state.street) {
        case Street::PreFlop:  label = "Pre-Flop";  break;
        case Street::Flop:     label = "Flop";      break;
        case Street::Turn:     label = "Turn";      break;
        case Street::River:    label = "River";     break;
        case Street::Showdown: label = "Showdown";  break;
    }
    sf::Text st(label, m_font, 17);
    st.setFillColor(sf::Color(180, 255, 180));
    st.setPosition(14.f, 12.f);
    m_window.draw(st);
}

void GameRenderer::drawShowdownHands(const GameState& state)
{
    if (state.street != Street::Showdown) return;

    std::vector<PlayerId> revealed;
    for (const auto& [id, _] : state.chipCounts) {
        if (state.foldedPlayers.count(id) == 0 && state.holeCards.count(id) > 0)
            revealed.push_back(id);
    }
    if (revealed.empty()) return;

    const int cols = std::min(4, std::max(1, static_cast<int>(revealed.size())));
    const float startX = 132.f;
    const float startY = 322.f;
    const float cellW = 132.f;
    const float cellH = 70.f;

    auto drawSmallCard = [&](const Card& card, sf::Vector2f pos) {
        sf::RectangleShape rect(sf::Vector2f(32.f, 44.f));
        rect.setPosition(pos);
        rect.setFillColor(sf::Color::White);
        rect.setOutlineColor(sf::Color(60, 60, 60));
        rect.setOutlineThickness(1.f);
        m_window.draw(rect);

        bool red = card.getSuit() == Suit::Hearts || card.getSuit() == Suit::Diamonds;
        sf::Text label(card.toShortString(), m_font, 13);
        label.setFillColor(red ? sf::Color(200, 0, 0) : sf::Color::Black);
        label.setPosition(pos.x + 3.f, pos.y + 4.f);
        m_window.draw(label);
    };

    for (int i = 0; i < static_cast<int>(revealed.size()); ++i) {
        PlayerId id = revealed[i];
        int row = i / cols;
        int col = i % cols;
        float x = startX + col * cellW;
        float y = startY + row * cellH;
        bool isWinner = state.showdownWinners.count(id) > 0;

        sf::RectangleShape box(sf::Vector2f(120.f, 60.f));
        box.setPosition(x, y);
        box.setFillColor(sf::Color(15, 55, 15, 210));
        box.setOutlineColor(isWinner ? sf::Color(255, 220, 80) : sf::Color(50, 120, 50));
        box.setOutlineThickness(isWinner ? 2.f : 1.f);
        m_window.draw(box);

        sf::Text name("P" + std::to_string(id) + (isWinner ? " WIN" : ""), m_font, 12);
        name.setFillColor(isWinner ? sf::Color(255, 240, 170) : sf::Color::White);
        name.setPosition(x + 4.f, y + 3.f);
        m_window.draw(name);

        const Hand& h = state.holeCards.at(id);
        drawSmallCard(h.first,  {x + 6.f,  y + 18.f});
        drawSmallCard(h.second, {x + 44.f, y + 18.f});
    }
}

} // namespace poker
