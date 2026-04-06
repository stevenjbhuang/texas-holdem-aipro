#include "PromptBuilder.hpp"
#include <sstream>

namespace poker {

std::string PromptBuilder::build(
    const PlayerView& view,
    const std::string& rulesText)
{
    std::stringstream ss;

    // 1. Optional rules context (for small/custom models that may not know poker)
    if (!rulesText.empty())
        ss << rulesText << "\n\n---\n\n";

    // 2. Game state
    ss << "You are playing a game of Texas Hold'em poker.\n";
    ss << "## Current game state:\n";
    ss << "Street: " << formatStreet(view.street) << "\n";
    ss << "Pot: $" << view.pot << "\n";

    // 3. Cards
    ss << "## Cards:\n";
    ss << "Your hand: " << formatHand(view.myHand) << "\n";
    ss << "Community cards: " << formatCommunityCards(view.communityCards) << "\n";

    // 4. Players
    ss << "## Players:\n";
    for (const auto& [playerId, chipCount] : view.chipCounts) {
        int bet = view.currentBets.count(playerId) ? view.currentBets.at(playerId) : 0;
        ss << "- Player " << playerId;
        if (playerId == view.myId)         ss << " (You)";
        if (playerId == view.dealerButton) ss << " (Dealer)";
        else if (playerId == view.smallBlindSeat) ss << " (Small Blind)";
        else if (playerId == view.bigBlindSeat)   ss << " (Big Blind)";
        if (view.foldedPlayers.count(playerId))
            ss << ": folded\n";
        else
            ss << ": $" << chipCount << ", Bet: $" << bet << "\n";
    }

    // 5. Action history
    if (!view.actionHistory.empty())
        ss << formatActionHistory(view.actionHistory, view.myId);

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

    ss << "\n## Your task:\n";
    ss << "Based on the current game state, choose the best action for you to take. Respond with one of the legal actions listed above, and if raising, specify the amount.\n";
    ss << "---\n\n";
    return ss.str();
}

std::string PromptBuilder::formatStreet(Street street) {
    switch (street) {
        case Street::PreFlop:  return "Pre-Flop";
        case Street::Flop:     return "Flop";
        case Street::Turn:     return "Turn";
        case Street::River:    return "River";
        case Street::Showdown: return "Showdown";
        default:               return "Unknown";
    }
}

std::string PromptBuilder::formatHand(const Hand& hand) {
    return hand.first.toShortString() + ", " + hand.second.toShortString();
}

std::string PromptBuilder::formatCommunityCards(const std::vector<Card>& communityCards) {
    if (communityCards.empty()) return "none";
    std::string result;
    for (size_t i = 0; i < communityCards.size(); ++i) {
        if (i > 0) result += ", ";
        result += communityCards[i].toShortString();
    }
    return result;
}

std::string PromptBuilder::formatActionHistory(const std::vector<ActionRecord>& history,
                                               PlayerId myId)
{
    std::stringstream ss;
    ss << "## Action History:\n";

    Street currentStreet = history[0].street;
    ss << "### " << formatStreet(currentStreet) << ":\n";

    for (const auto& rec : history) {
        if (rec.street != currentStreet) {
            currentStreet = rec.street;
            ss << "### " << formatStreet(currentStreet) << ":\n";
        }

        std::string playerLabel = (rec.player == myId)
            ? "You"
            : "Player " + std::to_string(rec.player);

        std::string actionStr;
        switch (rec.action.type) {
            case Action::Type::Fold:
                actionStr = "Fold";
                break;
            case Action::Type::Call:
                actionStr = (rec.action.amount == 0) ? "Check"
                                                     : "Call $" + std::to_string(rec.action.amount);
                break;
            case Action::Type::Raise:
                actionStr = "Raise $" + std::to_string(rec.action.amount);
                break;
        }

        ss << "- " << playerLabel << ": " << actionStr << "\n";
    }

    return ss.str();
}

std::string PromptBuilder::parseSystemPrompt(const std::string& fileContent)
{
    static const std::string kHeader = "## System Prompt";
    auto pos = fileContent.find(kHeader);
    if (pos == std::string::npos)
        return fileContent;

    pos = fileContent.find('\n', pos);
    if (pos == std::string::npos)
        return fileContent;
    ++pos;

    auto end = fileContent.find("\n##", pos);
    std::string section = (end == std::string::npos)
        ? fileContent.substr(pos)
        : fileContent.substr(pos, end - pos);

    auto first = section.find_first_not_of(" \t\n\r");
    auto last  = section.find_last_not_of(" \t\n\r");
    if (first == std::string::npos) return fileContent;
    return section.substr(first, last - first + 1);
}

} // namespace poker
