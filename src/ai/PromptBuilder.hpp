#pragma once
#include "core/PlayerView.hpp"
#include <string>

namespace poker {

class PromptBuilder
{
public:
    static std::string build(
        const PlayerView& view,
        const std::string& personalityText,
        const std::string& rulesText = ""
    );

private:
    static std::string formatStreet(Street street);
    static std::string formatCard(const Card& card);
    static std::string formatHand(const Hand& hand);
    static std::string formatCommunityCards(const std::vector<Card>& communityCards);
};

}                  