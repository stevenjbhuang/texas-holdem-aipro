#include "AIPlayer.hpp"
#include "ai/PromptBuilder.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace poker {

AIPlayer::AIPlayer(PlayerId id, const std::string& name, ILLMClient& client, const std::string& personalityText)
    : m_id(id), m_name(name), m_client(client), m_personalityText(personalityText) {}


void AIPlayer::dealHoleCards(const Hand& cards) {
    m_hand = cards;
}

Action AIPlayer::getAction(const PlayerView& view) {
    if (m_client.isStopped()) {
        return fallbackAction();
    }

    std::string prompt = PromptBuilder::build(view, m_personalityText);
    std::string response = m_client.sendPrompt(prompt);
    
    int retryCount = 0;
    // Simple retry logic for empty responses, which can happen due to LLM errors or timeouts
    while (response.empty() && retryCount < 3 && !m_client.isStopped()) {
        std::cerr << "Received empty response from LLM, retrying..." << std::endl;
        response = m_client.sendPrompt(prompt);
        retryCount++;
    }
    return parseResponse(response);
}

Action AIPlayer::parseResponse(const std::string& response) const {
    std::string upper = response;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper.find("FOLD") != std::string::npos) return Action{Action::Type::Fold};
    if (upper.find("CALL") != std::string::npos) return Action{Action::Type::Call};

    auto pos = upper.find("RAISE");
    if (pos != std::string::npos) {
        try {
            int amount = std::stoi(upper.substr(pos + 6));
            return Action{Action::Type::Raise, amount};
        } catch (...) {
            return fallbackAction();
        }
    }

    return fallbackAction();
}

Action AIPlayer::fallbackAction() const {
    return Action{Action::Type::Call};
}

}