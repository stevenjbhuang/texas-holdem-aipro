#pragma once
#include <SFML/Graphics.hpp>

namespace poker {

// The game always renders into a fixed 800×700 logical canvas.
// This view fits that canvas inside the actual window while preserving
// the aspect ratio — adding black bars (pillar-box or letter-box) as needed.
//
// Apply once after window creation and again on every sf::Event::Resized.
// For coordinate hit-testing always use window.mapPixelToCoords() rather
// than raw event pixel values so the bars are accounted for.

static constexpr float LOGICAL_W = 800.f;
static constexpr float LOGICAL_H = 700.f;

inline sf::View computeLetterboxView(sf::Vector2u windowSize)
{
    float wAspect = static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y);
    float lAspect = LOGICAL_W / LOGICAL_H;

    sf::View view(sf::FloatRect(0.f, 0.f, LOGICAL_W, LOGICAL_H));
    if (wAspect > lAspect) {
        float ratio = lAspect / wAspect;
        view.setViewport(sf::FloatRect((1.f - ratio) / 2.f, 0.f, ratio, 1.f));
    } else {
        float ratio = wAspect / lAspect;
        view.setViewport(sf::FloatRect(0.f, (1.f - ratio) / 2.f, 1.f, ratio));
    }
    return view;
}

// Letterbox variant for when the game area occupies only the left gameAreaW
// pixels of the window (e.g. the rest is a side panel).
// Viewport coordinates are always expressed as fractions of the full window.
inline sf::View computeLetterboxView(sf::Vector2u windowSize, float gameAreaW)
{
    float winW    = static_cast<float>(windowSize.x);
    float winH    = static_cast<float>(windowSize.y);
    float lAspect = LOGICAL_W / LOGICAL_H;
    float aAspect = gameAreaW / winH;   // aspect of the available game area

    sf::View view(sf::FloatRect(0.f, 0.f, LOGICAL_W, LOGICAL_H));

    if (aAspect > lAspect) {
        // Area is wider than logical canvas → pillar-box within game area
        float ratio = lAspect / aAspect;
        float vpW   = (gameAreaW / winW) * ratio;
        float vpX   = (gameAreaW / winW) * (1.f - ratio) / 2.f;
        view.setViewport(sf::FloatRect(vpX, 0.f, vpW, 1.f));
    } else {
        // Area is taller than logical canvas → letter-box within game area
        float ratio = aAspect / lAspect;
        float vpW   = gameAreaW / winW;
        float vpY   = (1.f - ratio) / 2.f;
        view.setViewport(sf::FloatRect(0.f, vpY, vpW, ratio));
    }
    return view;
}

} // namespace poker
