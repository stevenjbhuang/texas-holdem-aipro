#include "ui/GameRenderer.hpp"
#include "ui/LetterboxView.hpp"
#include <algorithm>
#include <iostream>
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
static constexpr int   MAX_SEATS      = 7;
static constexpr float PANEL_W        = 130.f;
static constexpr float PANEL_H        =  52.f;
static constexpr float CARD_W         =  50.f;
static constexpr float CARD_H         =  70.f;
static constexpr float CARD_GAP       =   8.f;
static constexpr float ACTION_BADGE_H =  22.f;

// Deck origin — conceptual position cards slide from.
static constexpr float DECK_X = 400.f;
static constexpr float DECK_Y = 148.f;

// Pot centre — where chips travel to and from.
static constexpr float POT_X = 400.f;
static constexpr float POT_Y = 248.f;

// Source card image dimensions (all assets/cards/*.png are this size)
static constexpr float SRC_CARD_W = 500.f;
static constexpr float SRC_CARD_H = 726.f;

// ── Asset name helpers ────────────────────────────────────────────────────────

static std::string rankName(Rank r) {
    switch (r) {
        case Rank::Two:   return "2";
        case Rank::Three: return "3";
        case Rank::Four:  return "4";
        case Rank::Five:  return "5";
        case Rank::Six:   return "6";
        case Rank::Seven: return "7";
        case Rank::Eight: return "8";
        case Rank::Nine:  return "9";
        case Rank::Ten:   return "10";
        case Rank::Jack:  return "jack";
        case Rank::Queen: return "queen";
        case Rank::King:  return "king";
        case Rank::Ace:   return "ace";
    }
    return "";
}

static std::string suitName(Suit s) {
    switch (s) {
        case Suit::Hearts:   return "hearts";
        case Suit::Diamonds: return "diamonds";
        case Suit::Clubs:    return "clubs";
        case Suit::Spades:   return "spades";
    }
    return "";
}

// ── Constructor ───────────────────────────────────────────────────────────────

GameRenderer::GameRenderer(sf::RenderWindow& window, sf::Font& font)
    : m_window(window), m_font(font)
{
    reloadAssets();
}

void GameRenderer::reloadAssets()
{
    if (!m_tableTexture.loadFromFile("assets/table/table_top.png"))
        std::cerr << "Warning: could not load assets/table/table_top.png — using plain felt\n";

    static constexpr Rank ranks[] = {
        Rank::Two, Rank::Three, Rank::Four, Rank::Five, Rank::Six,
        Rank::Seven, Rank::Eight, Rank::Nine, Rank::Ten,
        Rank::Jack, Rank::Queen, Rank::King, Rank::Ace
    };
    static constexpr Suit suits[] = {
        Suit::Hearts, Suit::Diamonds, Suit::Clubs, Suit::Spades
    };

    for (Suit suit : suits) {
        for (Rank rank : ranks) {
            std::string path = "assets/cards/" + rankName(rank) + "_of_" + suitName(suit) + ".png";
            if (!m_cardTextures[{rank, suit}].loadFromFile(path))
                std::cerr << "Warning: could not load " << path << "\n";
        }
    }

    if (!m_cardBackTexture.loadFromFile("assets/cards/black_joker.png"))
        std::cerr << "Warning: could not load assets/cards/black_joker.png\n";

    if (!m_chipTexture.loadFromFile("assets/chips/chip_blue_top_large.png"))
        std::cerr << "Warning: could not load chip texture for animations\n";
}

// ── seatPos ───────────────────────────────────────────────────────────────────
// Returns the screen position of a player's seat panel, using the same ordering
// as render(): human → SEATS[0], others in ascending PlayerId order (std::map).

sf::Vector2f GameRenderer::seatPos(PlayerId id, const GameState& state) const
{
    if (id == m_humanId) return SEATS[0];
    int idx = 1;
    for (auto& [pid, _] : state.chipCounts) {
        if (pid == m_humanId) continue;
        if (pid == id) return (idx < MAX_SEATS) ? SEATS[idx] : SEATS[MAX_SEATS - 1];
        ++idx;
    }
    return SEATS[0];
}

// ── detectDelta ───────────────────────────────────────────────────────────────

void GameRenderer::detectDelta(const GameState& curr,
                                AnimationManager& animMgr,
                                SoundManager& soundMgr)
{
    // First frame: no previous state to diff against.
    if (m_prevSnapshot.chipCounts.empty()) {
        m_prevSnapshot = curr;
        return;
    }

    // Street change: clear stale badge animations before the new street begins.
    if (curr.street != m_prevSnapshot.street)
        animMgr.clear();

    // ── New community cards ───────────────────────────────────────────────────
    int prevCC = static_cast<int>(m_prevSnapshot.communityCards.size());
    int currCC = static_cast<int>(curr.communityCards.size());
    if (currCC > prevCC) {
        soundMgr.post(SoundEvent::Deal);
        for (int i = prevCC; i < currCC; ++i) {
            float x = 259.f + i * (CARD_W + CARD_GAP);
            Anim a;
            a.type     = AnimType::SlideCard;
            a.from     = {DECK_X, DECK_Y};
            a.to       = {x, 265.f};
            a.card     = curr.communityCards[i];
            a.duration = 0.25f;
            a.delay    = (i - prevCC) * 0.15f;
            a.maskKey  = "card:community:" + std::to_string(i);
            animMgr.enqueue(a);
        }
    }

    // ── New hole cards (start of hand) ────────────────────────────────────────
    bool dealtHoles = false;
    float holeDelay = 0.f;
    for (auto& [pid, hand] : curr.holeCards) {
        if (m_prevSnapshot.holeCards.count(pid) > 0) continue;

        if (!dealtHoles) {
            soundMgr.post(SoundEvent::Deal);
            dealtHoles = true;
        }

        sf::Vector2f pos    = seatPos(pid, curr);
        bool         human  = (pid == m_humanId);
        float        cardY  = pos.y + PANEL_H + ACTION_BADGE_H + 4.f;

        sf::Vector2f to0 = human ? sf::Vector2f{346.f, 516.f}
                                 : sf::Vector2f{pos.x + 2.f, cardY};
        sf::Vector2f to1 = human ? sf::Vector2f{404.f, 516.f}
                                 : sf::Vector2f{pos.x + 2.f + CARD_W + CARD_GAP, cardY};

        Anim a0;
        a0.type     = AnimType::SlideCard;
        a0.from     = {DECK_X, DECK_Y};
        a0.to       = to0;
        a0.card     = hand.first;
        a0.faceDown = !human;
        a0.duration = 0.25f;
        a0.delay    = holeDelay;
        a0.maskKey  = "card:hole:" + std::to_string(pid) + ":0";
        animMgr.enqueue(a0);

        Anim a1     = a0;
        a1.to       = to1;
        a1.card     = hand.second;
        a1.delay    = holeDelay + 0.12f;
        a1.maskKey  = "card:hole:" + std::to_string(pid) + ":1";
        animMgr.enqueue(a1);

        holeDelay += 0.20f;
    }

    // ── Chip movements (player bets / raises) ─────────────────────────────────
    for (auto& [pid, bet] : curr.currentBets) {
        int prevBet = m_prevSnapshot.currentBets.count(pid)
                      ? m_prevSnapshot.currentBets.at(pid) : 0;
        if (bet > prevBet) {
            Anim a;
            a.type     = AnimType::SlideChip;
            a.from     = seatPos(pid, curr);
            a.to       = {POT_X, POT_Y};
            a.duration = 0.30f;
            animMgr.enqueue(a);
            soundMgr.post(SoundEvent::Chip);
        }
    }

    // ── Action badges (non-human players only) ────────────────────────────────
    for (auto& [pid, action] : curr.lastActions) {
        if (pid == m_humanId) continue;

        bool isNew     = (m_prevSnapshot.lastActions.count(pid) == 0);
        bool isChanged = !isNew &&
                         (m_prevSnapshot.lastActions.at(pid).type   != action.type ||
                          m_prevSnapshot.lastActions.at(pid).amount != action.amount);
        if (!isNew && !isChanged) continue;

        std::string  badgeText;
        sf::Color    badgeColor;
        switch (action.type) {
            case Action::Type::Fold:
                badgeText  = "Fold";
                badgeColor = sf::Color(220, 80, 80);
                soundMgr.post(SoundEvent::Fold);
                break;
            case Action::Type::Call:
                if (action.amount == 0) {
                    badgeText  = "Check";
                    badgeColor = sf::Color(100, 200, 255);
                } else {
                    badgeText  = "Call $" + std::to_string(action.amount);
                    badgeColor = sf::Color(80, 200, 120);
                }
                break;
            case Action::Type::Raise:
                badgeText  = "Raise $" + std::to_string(action.amount);
                badgeColor = sf::Color(255, 210, 60);
                break;
        }

        Anim a;
        a.type     = AnimType::FadeBadge;
        a.from     = seatPos(pid, curr);
        a.text     = badgeText;
        a.color    = badgeColor;
        a.duration = 1.2f;
        animMgr.enqueue(a);
    }

    // ── Winner spotlight ──────────────────────────────────────────────────────
    if (!curr.showdownWinners.empty() && m_prevSnapshot.showdownWinners.empty()) {
        // Use prev pot value in case it was already distributed in this snapshot.
        int potAmt = (m_prevSnapshot.pot > 0) ? m_prevSnapshot.pot : curr.pot;
        soundMgr.post(SoundEvent::Win);

        for (PlayerId winner : curr.showdownWinners) {
            sf::Vector2f wPos = seatPos(winner, curr);

            Anim chip;
            chip.type     = AnimType::SlideChip;
            chip.from     = {POT_X, POT_Y};
            chip.to       = wPos;
            chip.duration = 0.35f;
            animMgr.enqueue(chip);

            Anim ft;
            ft.type     = AnimType::FloatText;
            ft.from     = wPos;
            ft.text     = "+$" + std::to_string(potAmt);
            ft.duration = 0.9f;
            animMgr.enqueue(ft);

            Anim fp;
            fp.type     = AnimType::FadePanel;
            fp.from     = wPos;
            fp.size     = {PANEL_W, PANEL_H};
            fp.duration = 0.8f;
            animMgr.enqueue(fp);
        }
    }

    m_prevSnapshot = curr;
}

// ── render ────────────────────────────────────────────────────────────────────

void GameRenderer::render(const GameState& state, PlayerId humanId,
                           AnimationManager& animMgr)
{
    m_humanId = humanId;
    m_window.clear(sf::Color::Black);

    drawTable();
    drawCommunityCards(state, animMgr);
    drawPot(state);
    drawStreetLabel(state);

    // Build player list: human at SEATS[0], others in ascending PlayerId order.
    // chipCounts is std::map so iteration order is deterministic — seat positions
    // are stable across frames as long as chipCounts stays a std::map.
    std::vector<PlayerId> players;
    players.push_back(humanId);
    for (auto& [id, _] : state.chipCounts)
        if (id != humanId) players.push_back(id);

    for (int i = 0; i < static_cast<int>(players.size()) && i < MAX_SEATS; ++i)
        drawPlayer(players[i], state, SEATS[i], players[i] == humanId, animMgr);

    // Human hole cards — skip any card still animating in.
    if (state.holeCards.count(humanId)) {
        const Hand& h = state.holeCards.at(humanId);
        std::string prefix = "card:hole:" + std::to_string(humanId);
        if (!animMgr.isMasked(prefix + ":0")) drawCard(h.first,  {346.f, 516.f});
        if (!animMgr.isMasked(prefix + ":1")) drawCard(h.second, {404.f, 516.f});
    }

    // "Thinking…" indicator.
    if (state.waitingForAction && state.activePlayer != humanId) {
        sf::Text thinking("Thinking...", m_font, 18);
        thinking.setFillColor(sf::Color::Yellow);
        thinking.setStyle(sf::Text::Italic);
        sf::FloatRect tb = thinking.getLocalBounds();
        thinking.setPosition(800.f - tb.width - 14.f, 12.f);
        m_window.draw(thinking);
    }

    // Draw all in-flight animations on top of the static scene.
    animMgr.draw(m_window, m_font, m_cardTextures, m_cardBackTexture, m_chipTexture);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void GameRenderer::drawTable()
{
    if (m_tableTexture.getSize().x > 0) {
        sf::Sprite table(m_tableTexture);
        float srcW  = static_cast<float>(m_tableTexture.getSize().x);
        float srcH  = static_cast<float>(m_tableTexture.getSize().y);
        float scale = std::min(LOGICAL_W / srcW, LOGICAL_H / srcH);
        table.setScale(scale, scale);
        table.setPosition((LOGICAL_W - srcW * scale) / 2.f, (LOGICAL_H - srcH * scale) / 2.f);
        m_window.draw(table);
    } else {
        sf::RectangleShape felt(sf::Vector2f(580.f, 270.f));
        felt.setPosition(110.f, 148.f);
        felt.setFillColor(sf::Color(0, 110, 0));
        felt.setOutlineColor(sf::Color(101, 67, 33));
        felt.setOutlineThickness(10.f);
        m_window.draw(felt);
    }
}

void GameRenderer::drawCard(const Card& card, sf::Vector2f pos)
{
    auto it = m_cardTextures.find({card.getRank(), card.getSuit()});
    if (it != m_cardTextures.end() && it->second.getSize().x > 0) {
        sf::Sprite sprite(it->second);
        sprite.setScale(CARD_W / SRC_CARD_W, CARD_H / SRC_CARD_H);
        sprite.setPosition(pos);
        m_window.draw(sprite);
    } else {
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
}

void GameRenderer::drawCardBack(sf::Vector2f pos)
{
    if (m_cardBackTexture.getSize().x > 0) {
        sf::Sprite sprite(m_cardBackTexture);
        sprite.setScale(CARD_W / SRC_CARD_W, CARD_H / SRC_CARD_H);
        sprite.setPosition(pos);
        m_window.draw(sprite);
    } else {
        sf::RectangleShape rect(sf::Vector2f(CARD_W, CARD_H));
        rect.setPosition(pos);
        rect.setFillColor(sf::Color(30, 50, 140));
        rect.setOutlineColor(sf::Color(60, 60, 60));
        rect.setOutlineThickness(1.f);
        m_window.draw(rect);
    }
}

void GameRenderer::drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos,
                               bool isHuman, AnimationManager& animMgr)
{
    bool folded      = state.foldedPlayers.count(id) > 0;
    bool active      = state.waitingForAction && state.activePlayer == id;
    int  chips       = state.chipCounts.count(id)       ? state.chipCounts.at(id)       : 0;
    int  bet         = state.currentBets.count(id)      ? state.currentBets.at(id)      : 0;
    int  contributed = state.totalContributed.count(id) ? state.totalContributed.at(id) : 0;
    bool busted      = (chips == 0 && bet == 0 && contributed == 0);

    sf::RectangleShape panel(sf::Vector2f(PANEL_W, PANEL_H));
    panel.setPosition(pos);
    if      (active) panel.setFillColor(sf::Color(200, 170,  20));
    else if (folded) panel.setFillColor(sf::Color( 60,  60,  60));
    else if (busted) panel.setFillColor(sf::Color(100,  20,  20));
    else             panel.setFillColor(sf::Color( 40,  80,  40));

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

    // Static action badge (always drawn; FadeBadge overlay in AnimationManager is additive).
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

        sf::RectangleShape backing(sf::Vector2f(tb.width + 8.f, tb.height + 6.f));
        backing.setPosition(bx, by);
        backing.setFillColor(sf::Color(20, 20, 20, 200));
        m_window.draw(backing);

        actionText.setFillColor(actionColor);
        actionText.setPosition(bx + 4.f, by + 2.f);
        m_window.draw(actionText);
    }

    // AI hole cards — skip any that are still animating in.
    if (!isHuman && !folded && state.holeCards.count(id)) {
        float cardY = pos.y + PANEL_H + ACTION_BADGE_H + 4.f;
        std::string prefix = "card:hole:" + std::to_string(id);

        if (state.street == Street::Showdown) {
            const Hand& h = state.holeCards.at(id);
            if (!animMgr.isMasked(prefix + ":0"))
                drawCard(h.first,  {pos.x + 2.f,               cardY});
            if (!animMgr.isMasked(prefix + ":1"))
                drawCard(h.second, {pos.x + 2.f + CARD_W + CARD_GAP, cardY});
        } else {
            if (!animMgr.isMasked(prefix + ":0"))
                drawCardBack({pos.x + 2.f,               cardY});
            if (!animMgr.isMasked(prefix + ":1"))
                drawCardBack({pos.x + 2.f + CARD_W + CARD_GAP, cardY});
        }
    }
}

void GameRenderer::drawCommunityCards(const GameState& state, AnimationManager& animMgr)
{
    // 5 cards × 50px + 4 gaps × 8px = 282px total; centre at x=400 → start at 259.
    float x = 259.f;
    int   i = 0;
    for (const Card& c : state.communityCards) {
        if (!animMgr.isMasked("card:community:" + std::to_string(i)))
            drawCard(c, {x, 265.f});
        x += CARD_W + CARD_GAP;
        ++i;
    }
    for (; i < 5; ++i) {
        sf::RectangleShape slot(sf::Vector2f(CARD_W, CARD_H));
        slot.setPosition(x, 265.f);
        slot.setFillColor(sf::Color(0, 0, 0, 80));
        slot.setOutlineColor(sf::Color(255, 255, 255, 60));
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

} // namespace poker
