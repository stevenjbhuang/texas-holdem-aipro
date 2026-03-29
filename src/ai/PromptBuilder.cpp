#include "PromptBuilder.hpp"
#include <sstream>

namespace poker {

std::string PromptBuilder::build(
    const PlayerView& view,
    const std::string& personalityText,
    const std::string& rulesText)
{

    std::stringstream ss;
    // 1. Personality
    ss << personalityText << "\n\n---\n\n";

    // 2. Optional rules context (for small/custom models that may not know poker)
    if (!rulesText.empty())
        ss << rulesText << "\n\n---\n\n";

    // 3. Game state
    ss << "## Current game state:\n";
    ss << "Street: " << formatStreet(view.street) << "\n";
    ss << "Pot: $" << view.pot << "\n";

    // 4. Cards
    ss << "## Cards:\n";
    ss << "Your hand: " << formatHand(view.myHand) << "\n";
    ss << "Community cards: " << formatCommunityCards(view.communityCards) << "\n";
    
    // 5. Opponent info
    ss << "## Opponent info:\n";
    for (const auto& [playerId, chipCount] : view.chipCounts) {
        int bet = view.currentBets.count(playerId) ? view.currentBets.at(playerId) : 0;
        ss << "- Player " << playerId;
        if (playerId == view.myId) {
            ss << " (You)";
        }
        if (playerId == view.dealerButton) {
            ss << " (Dealer)";
        } else if (playerId == view.smallBlindSeat) {
            ss << " (Small Blind)";
        } else if (playerId == view.bigBlindSeat) {
            ss << " (Big Blind)";
        }
        if (view.foldedPlayers.count(playerId)) {
            ss << ": folded" << "\n";
        }
        else {
            ss << ": $" << chipCount << ", Bet: $" << bet << "\n";
        }
        ss << "\n";
    }

    // 6. Legal actions
    ss << "## Legal Actions\n";
    ss << "- FOLD\n";
    if (view.legal.canCheck)
        ss << "- CHECK\n";
    if (view.legal.canCall)
        ss << "- CALL $" << view.legal.callCost << "\n";
    if (view.legal.canRaise)
        ss << "- RAISE <amount>  (min: $" << view.legal.minRaiseTo
           << ", max: $" << view.legal.maxRaiseTo << ")\n";

    ss << "\n---\n\n";
    ss << "Respond with exactly one action from Legal Actions above. No explanation.";
    return ss.str();
}

std::string PromptBuilder::formatStreet(Street street) {
    switch (street) {
        case Street::PreFlop: return "Pre-Flop";
        case Street::Flop: return "Flop";
        case Street::Turn: return "Turn";
        case Street::River: return "River";
        case Street::Showdown: return "Showdown";
        default: return "Unknown Street";
    }
}

std::string PromptBuilder::formatCard(const Card& card) {
    return card.toShortString();
}

std::string PromptBuilder::formatHand(const Hand& hand) {
    return formatCard(hand.first) + ", " + formatCard(hand.second);
}

std::string PromptBuilder::formatCommunityCards(const std::vector<Card>& communityCards) {
    if (communityCards.empty()) {
        return "none";
    }
    std::string result;
    for (size_t i = 0; i < communityCards.size(); ++i) {
        result += formatCard(communityCards[i]);
        if (i < communityCards.size() - 1) {
            result += ", ";
        }
    }
    return result;
}

}