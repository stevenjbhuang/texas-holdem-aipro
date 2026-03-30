#pragma once
#include "IPlayer.hpp"
#include "ai/ILLMClient.hpp"
#include "string"

namespace poker {


class AIPlayer : public IPlayer {

public:
    AIPlayer(
        PlayerId id,
        const std::string& name,
        ILLMClient& client,
        const std::string& personalityText
    );

    PlayerId getId() const override { return m_id; };
    std::string getName() const override { return m_name; };
    void dealHoleCards(const Hand& cards) override;
    Action getAction(const PlayerView& view) override;

private:
    Action parseResponse(const std::string& response) const;
    Action fallbackAction() const;

    PlayerId m_id;
    std::string m_name;
    Hand m_hand;

    ILLMClient& m_client;
    std::string m_personalityText;

};



}