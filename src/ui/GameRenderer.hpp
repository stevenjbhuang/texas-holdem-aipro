#pragma once
#include "core/GameState.hpp"
#include "ui/AnimationManager.hpp"
#include "ui/SoundManager.hpp"
#include <SFML/Graphics.hpp>
#include <map>
#include <utility>

namespace poker {

class GameRenderer {
public:
    GameRenderer(sf::RenderWindow& window, sf::Font& font);

    // Diff curr against the previous snapshot and enqueue any new animations/sounds.
    // Call once per frame before render().
    void detectDelta(const GameState& curr,
                     AnimationManager& animMgr,
                     SoundManager& soundMgr);

    // Draws one frame from a state snapshot.
    // snapshot is a value copy — safe to read with no locks held.
    void render(const GameState& snapshot, PlayerId humanId,
                AnimationManager& animMgr);

    // Reloads all GPU textures. Must be called after window.create() because
    // recreating the window destroys the OpenGL context and invalidates textures.
    void reloadAssets();

private:
    // Returns the top-left screen position of a player's seat panel.
    sf::Vector2f seatPos(PlayerId id, const GameState& state) const;

    void drawTable();
    void drawPlayer(PlayerId id, const GameState& state, sf::Vector2f pos,
                    bool isHuman, AnimationManager& animMgr);
    void drawCard(const Card& card, sf::Vector2f pos);
    void drawCardBack(sf::Vector2f pos);
    void drawCommunityCards(const GameState& state, AnimationManager& animMgr);
    void drawPot(const GameState& state);
    void drawStreetLabel(const GameState& state);

    sf::RenderWindow& m_window;
    sf::Font&         m_font;

    sf::Texture m_tableTexture;
    std::map<std::pair<Rank, Suit>, sf::Texture> m_cardTextures;
    sf::Texture m_cardBackTexture;
    sf::Texture m_chipTexture;

    // Stored between frames for delta detection.
    GameState  m_prevSnapshot;
    PlayerId   m_humanId = 0;
};

} // namespace poker
