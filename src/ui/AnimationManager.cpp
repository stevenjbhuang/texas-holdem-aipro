#include "ui/AnimationManager.hpp"
#include <algorithm>

namespace poker {

// Card display size — must match GameRenderer constants.
static constexpr float CARD_W     = 50.f;
static constexpr float CARD_H     = 70.f;
static constexpr float SRC_CARD_W = 500.f;
static constexpr float SRC_CARD_H = 726.f;

// Chip display size for in-flight sprites.
static constexpr float CHIP_SIZE  = 22.f;

void AnimationManager::enqueue(Anim anim)
{
    if (!anim.maskKey.empty())
        m_masked.insert(anim.maskKey);
    m_anims.push_back(std::move(anim));
}

void AnimationManager::update(sf::Time dt)
{
    float dtSec = dt.asSeconds();

    m_anims.erase(
        std::remove_if(m_anims.begin(), m_anims.end(), [&](Anim& a) {
            if (a.delay > 0.f) {
                a.delay -= dtSec;
                return false;
            }
            a.elapsed += dtSec;
            if (a.elapsed >= a.duration) {
                if (!a.maskKey.empty())
                    m_masked.erase(a.maskKey);
                return true;  // remove
            }
            return false;
        }),
        m_anims.end()
    );
}

void AnimationManager::draw(sf::RenderWindow& window,
                            sf::Font& font,
                            const std::map<std::pair<Rank, Suit>, sf::Texture>& cardTextures,
                            const sf::Texture& cardBackTexture,
                            const sf::Texture& chipTexture)
{
    for (const Anim& a : m_anims) {
        if (a.delay > 0.f) continue;  // not started yet

        float t = (a.duration > 0.f) ? std::min(a.elapsed / a.duration, 1.f) : 1.f;

        switch (a.type) {

        case AnimType::SlideCard: {
            sf::Vector2f pos = a.from + (a.to - a.from) * t;

            if (a.faceDown) {
                if (cardBackTexture.getSize().x > 0) {
                    sf::Sprite s(cardBackTexture);
                    s.setScale(CARD_W / SRC_CARD_W, CARD_H / SRC_CARD_H);
                    s.setPosition(pos);
                    window.draw(s);
                }
            } else {
                auto it = cardTextures.find({a.card.getRank(), a.card.getSuit()});
                if (it != cardTextures.end() && it->second.getSize().x > 0) {
                    sf::Sprite s(it->second);
                    s.setScale(CARD_W / SRC_CARD_W, CARD_H / SRC_CARD_H);
                    s.setPosition(pos);
                    window.draw(s);
                }
            }
            break;
        }

        case AnimType::SlideChip: {
            sf::Vector2f pos = a.from + (a.to - a.from) * t;
            if (chipTexture.getSize().x > 0) {
                sf::Sprite s(chipTexture);
                float scale = CHIP_SIZE / static_cast<float>(chipTexture.getSize().x);
                s.setScale(scale, scale);
                s.setPosition(pos);
                window.draw(s);
            } else {
                // Fallback: small gold circle via rectangle
                sf::RectangleShape r({CHIP_SIZE, CHIP_SIZE});
                r.setPosition(pos);
                r.setFillColor(sf::Color(200, 170, 30));
                window.draw(r);
            }
            break;
        }

        case AnimType::FadeBadge: {
            sf::Uint8 alpha = static_cast<sf::Uint8>(255 * (1.f - t));
            float bx = a.from.x + 2.f;
            float by = a.from.y + 52.f + 2.f;  // below panel (PANEL_H=52)

            sf::Text text(a.text, font, 11);
            sf::FloatRect tb = text.getLocalBounds();

            sf::RectangleShape backing({tb.width + 8.f, tb.height + 6.f});
            backing.setPosition(bx, by);
            backing.setFillColor(sf::Color(20, 20, 20, alpha));
            window.draw(backing);

            sf::Color col = a.color;
            col.a = alpha;
            text.setFillColor(col);
            text.setPosition(bx + 4.f, by + 2.f);
            window.draw(text);
            break;
        }

        case AnimType::FloatText: {
            sf::Uint8 alpha = static_cast<sf::Uint8>(255 * (1.f - t));
            float yOffset = -30.f * t;

            sf::Text text(a.text, font, 15);
            sf::FloatRect tb = text.getLocalBounds();
            text.setPosition(a.from.x + 65.f - tb.width / 2.f,
                             a.from.y + yOffset);
            text.setFillColor(sf::Color(255, 220, 60, alpha));
            window.draw(text);
            break;
        }

        case AnimType::FadePanel: {
            sf::Uint8 alpha = static_cast<sf::Uint8>(150 * (1.f - t));
            sf::RectangleShape rect(a.size);
            rect.setPosition(a.from);
            rect.setFillColor(sf::Color(255, 200, 50, alpha));
            window.draw(rect);
            break;
        }
        }
    }
}

bool AnimationManager::isMasked(const std::string& key) const
{
    return m_masked.count(key) > 0;
}

bool AnimationManager::hasActive() const
{
    return !m_anims.empty();
}

void AnimationManager::clear()
{
    m_anims.clear();
    m_masked.clear();
}

} // namespace poker
