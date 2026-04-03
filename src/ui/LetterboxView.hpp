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

} // namespace poker
