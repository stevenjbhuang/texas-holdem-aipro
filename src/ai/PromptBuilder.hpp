#pragma once
#include "core/PlayerView.hpp"
#include <string>

namespace poker {

class PromptBuilder
{
public:
    static std::string build(
        const PlayerView& view,
        const std::string& rulesText = ""
    );

    // Extracts the content of the "## System Prompt" section from a personality file.
    // Returns the full file content unchanged if the section is not found.
    static std::string parseSystemPrompt(const std::string& fileContent);

private:
    static std::string formatStreet(Street street);
    static std::string formatHand(const Hand& hand);
    static std::string formatCommunityCards(const std::vector<Card>& communityCards);
    static std::string formatActionHistory(const std::vector<ActionRecord>& history,
                                           PlayerId myId);
};

}                  