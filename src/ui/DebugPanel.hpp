#pragma once
#include "LLMDebugLog.hpp"
#include <SFML/Graphics.hpp>
#include <ctime>
#include <string>
#include <vector>

namespace poker {

class DebugPanel {
public:
    static constexpr float WIDTH = 400.f;

    DebugPanel(sf::RenderWindow& window, sf::Font& font, LLMDebugLog& log);

    // Draw the panel into the right WIDTH pixels of the window.
    // Saves and restores the window view around its own drawing.
    void draw();

    // Call when the window is resized so the viewport stays correct.
    void onResize(unsigned windowW, unsigned windowH);

    // Scroll content by delta pixels (positive = scroll down / older content).
    void scroll(float delta);

private:
    sf::RenderWindow& m_window;
    sf::Font&         m_font;
    LLMDebugLog&      m_log;
    sf::View          m_view;
    float             m_height      = 700.f; // current physical panel height
    float             m_scrollY     = 0.f;   // current scroll offset in logical px
    float             m_contentHeight = 0.f; // total content height from last draw

    struct CachedEntry {
        std::time_t                      timestamp;
        std::vector<std::string>         requestLines;
        std::vector<std::string>         responseLines;
    };

    std::vector<CachedEntry> m_cache;  // rebuilt only when snapshot timestamps change

    std::vector<std::string> wrapText(const std::string& text,
                                      unsigned charSize,
                                      float maxWidth) const;

    float drawBlock(const std::string& label,
                    const std::vector<std::string>& lines,
                    float x, float y,
                    const sf::Color& labelColor,
                    const sf::Color& bodyColor);
};

} // namespace poker
