#pragma once
#include "core/Card.hpp"
#include "core/Types.hpp"
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace poker {

enum class AnimType {
    SlideCard,  // card sprite slides from→to
    SlideChip,  // chip sprite slides from→to
    FadeBadge,  // action badge text fades out at 'from'
    FloatText,  // "+$N" text floats upward and fades
    FadePanel,  // golden rect overlay at 'from' with 'size' fades out
};

struct Anim {
    AnimType     type     = AnimType::SlideCard;
    sf::Vector2f from;
    sf::Vector2f to;
    sf::Vector2f size;        // FadePanel: width/height of the panel rect
    float        duration = 0.25f;
    float        elapsed  = 0.f;
    float        delay    = 0.f;  // seconds before this animation starts ticking

    Card         card;            // SlideCard: which card face to draw
    bool         faceDown = false;// SlideCard: draw back instead of face
    sf::Color    color;           // FadeBadge: text colour
    std::string  maskKey;         // non-empty → removed from mask when animation completes
    std::string  text;            // FadeBadge/FloatText: display string
};

class AnimationManager {
public:
    // Add an animation. If anim.maskKey is non-empty, the key is added to the
    // masked set immediately so GameRenderer can skip the static draw right away.
    void enqueue(Anim anim);

    // Advance all active animations by dt. Completed animations are removed;
    // their maskKeys are erased from the masked set.
    void update(sf::Time dt);

    // Draw all in-flight animations on top of the static scene.
    // cardTextures and chipTexture are owned by GameRenderer and passed through.
    void draw(sf::RenderWindow& window,
              sf::Font& font,
              const std::map<std::pair<Rank, Suit>, sf::Texture>& cardTextures,
              const sf::Texture& cardBackTexture,
              const sf::Texture& chipTexture);

    // Returns true if key is in the masked set (i.e. an animation is covering it).
    bool isMasked(const std::string& key) const;

    // Returns true if any animation is currently active or queued.
    bool hasActive() const;

    // Remove all active animations and clear all masks. Call on street/hand reset.
    void clear();

private:
    std::vector<Anim>               m_anims;
    std::unordered_set<std::string> m_masked;
};

} // namespace poker
