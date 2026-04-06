#include "DebugPanel.hpp"
#include <algorithm>
#include <sstream>

namespace poker {

static constexpr unsigned FONT_SIZE_BODY = 11;
static constexpr float    PAD              = 8.f;
static constexpr float    LINE_SPACING     = 14.f;

static const sf::Color BG_COLOR     {18,  20,  30,  255};
static const sf::Color BORDER_COLOR {60,  65,  90,  255};
static const sf::Color HEADER_COLOR {180, 200, 255, 255};
static const sf::Color LABEL_COLOR  {120, 200, 140, 255};
static const sf::Color BODY_COLOR   {200, 200, 210, 255};
static const sf::Color RESP_COLOR   {255, 220, 100, 255};
static const sf::Color SEP_COLOR    {50,  55,  80,  255};
static const sf::Color TIME_COLOR   {100, 110, 140, 255};

// ─────────────────────────────────────────────────────────────────────────────

DebugPanel::DebugPanel(sf::RenderWindow& window, sf::Font& font, LLMDebugLog& log)
    : m_window(window), m_font(font), m_log(log)
{
    onResize(window.getSize().x, window.getSize().y);
}

void DebugPanel::onResize(unsigned windowW, unsigned windowH) {
    // Logical size matches the physical pixel size of the panel area so that
    // text always renders 1:1 — no stretch when the window height changes.
    m_height = static_cast<float>(windowH);
    float vpX = (static_cast<float>(windowW) - WIDTH) / static_cast<float>(windowW);
    float vpW = WIDTH / static_cast<float>(windowW);
    m_view.setViewport(sf::FloatRect(vpX, 0.f, vpW, 1.f));
    // Re-clamp scroll after resize since visible area may have changed.
    scroll(0.f);
}

void DebugPanel::scroll(float delta) {
    m_scrollY += delta;
    float maxScroll = std::max(0.f, m_contentHeight - m_height);
    m_scrollY = std::max(0.f, std::min(m_scrollY, maxScroll));
}

// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> DebugPanel::wrapText(const std::string& text,
                                               unsigned charSize,
                                               float maxWidth) const {
    std::vector<std::string> result;
    sf::Text probe("", m_font, charSize);
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        std::string current;
        for (char c : line) {
            current += c;
            probe.setString(current);
            if (probe.getLocalBounds().width > maxWidth && current.size() > 1) {
                result.push_back(current.substr(0, current.size() - 1));
                current = std::string(1, c);
            }
        }
        result.push_back(current);
    }
    return result;
}

float DebugPanel::drawBlock(const std::string& label,
                             const std::vector<std::string>& lines,
                             float x, float y,
                             const sf::Color& labelColor,
                             const sf::Color& bodyColor) {
    sf::Text lbl(label, m_font, FONT_SIZE_BODY);
    lbl.setFillColor(labelColor);
    lbl.setPosition(x, y);
    m_window.draw(lbl);
    y += LINE_SPACING;

    for (const auto& l : lines) {
        if (y > m_scrollY + m_height - LINE_SPACING) break; // clip at panel bottom
        sf::Text t(l, m_font, FONT_SIZE_BODY);
        t.setFillColor(bodyColor);
        t.setPosition(x, y);
        m_window.draw(t);
        y += LINE_SPACING;
    }
    return y;
}

// ─────────────────────────────────────────────────────────────────────────────

void DebugPanel::draw() {
    sf::View savedView = m_window.getView();

    // Apply scroll: shift the visible region down by m_scrollY.
    m_view.reset(sf::FloatRect(0.f, m_scrollY, WIDTH, m_height));
    m_window.setView(m_view);

    // Background — drawn in scrolled coords so it covers the visible area.
    sf::RectangleShape bg(sf::Vector2f(WIDTH, m_height + m_scrollY));
    bg.setFillColor(BG_COLOR);
    bg.setOutlineColor(BORDER_COLOR);
    bg.setOutlineThickness(1.f);
    m_window.draw(bg);

    // Title bar — pinned to top of visible area (scrolled coords).
    sf::RectangleShape titleBar(sf::Vector2f(WIDTH, 24.f));
    titleBar.setFillColor(BORDER_COLOR);
    titleBar.setPosition(0.f, m_scrollY);
    m_window.draw(titleBar);

    sf::Text title("LLM Debug  —  full log: logs/debug_llm.log", m_font, FONT_SIZE_BODY);
    title.setFillColor(HEADER_COLOR);
    title.setPosition(PAD, m_scrollY + 5.f);
    m_window.draw(title);

    // Rebuild wrapped-line cache only when the snapshot has new entries.
    auto entries = m_log.snapshot();
    const float maxW = WIDTH - PAD * 2 - 8.f;

    bool cacheStale = (m_cache.size() != entries.size());
    if (!cacheStale) {
        for (size_t i = 0; i < m_cache.size(); ++i)
            if (m_cache[i].timestamp != entries[i].timestamp) { cacheStale = true; break; }
    }
    if (cacheStale) {
        m_cache.clear();
        for (const auto& e : entries) {
            CachedEntry c;
            c.timestamp     = e.timestamp;
            c.requestLines  = wrapText(e.userMsg, FONT_SIZE_BODY, maxW);
            c.responseLines = wrapText(e.response.empty() ? "(empty)" : e.response,
                                       FONT_SIZE_BODY, maxW);
            m_cache.push_back(std::move(c));
        }
    }

    // Entries (newest first) — drawn in content coords (no scroll offset needed).
    float y = 30.f;
    const float contentLeft = PAD;
    int idx = static_cast<int>(m_cache.size());

    for (size_t i = 0; i < m_cache.size(); ++i) {
        const auto& cached = m_cache[i];

        // Separator + timestamp
        sf::RectangleShape sep(sf::Vector2f(WIDTH - PAD * 2, 1.f));
        sep.setFillColor(SEP_COLOR);
        sep.setPosition(contentLeft, y);
        m_window.draw(sep);
        y += 4.f;

        char timebuf[16];
        std::tm* tm = std::localtime(&cached.timestamp);
        std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", tm);

        sf::Text ts(std::string("── #") + std::to_string(idx--) + "  " + timebuf,
                    m_font, FONT_SIZE_BODY);
        ts.setFillColor(TIME_COLOR);
        ts.setPosition(contentLeft, y);
        m_window.draw(ts);
        y += LINE_SPACING + 2.f;

        y = drawBlock("REQUEST:",  cached.requestLines,  contentLeft, y, LABEL_COLOR, BODY_COLOR);
        y += 2.f;

        y = drawBlock("RESPONSE:", cached.responseLines, contentLeft, y, LABEL_COLOR, RESP_COLOR);
        y += 6.f;
    }

    if (m_cache.empty()) {
        sf::Text waiting("Waiting for LLM requests...", m_font, FONT_SIZE_BODY);
        waiting.setFillColor(TIME_COLOR);
        waiting.setPosition(PAD, 40.f);
        m_window.draw(waiting);
    }

    // Track total content height for scroll clamping.
    m_contentHeight = y;

    // ── Scrollbar ────────────────────────────────────────────────────────────
    if (m_contentHeight > m_height) {
        constexpr float SB_W = 5.f;
        float trackH   = m_height;
        float thumbH   = std::max(20.f, trackH * (m_height / m_contentHeight));
        float thumbY   = m_scrollY + (m_scrollY / std::max(1.f, m_contentHeight - m_height))
                         * (trackH - thumbH);

        sf::RectangleShape track(sf::Vector2f(SB_W, trackH));
        track.setFillColor({30, 35, 50, 255});
        track.setPosition(WIDTH - SB_W, m_scrollY);
        m_window.draw(track);

        sf::RectangleShape thumb(sf::Vector2f(SB_W, thumbH));
        thumb.setFillColor({90, 100, 140, 255});
        thumb.setPosition(WIDTH - SB_W, thumbY);
        m_window.draw(thumb);
    }

    m_window.setView(savedView);
}

} // namespace poker
