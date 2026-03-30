#pragma once
#include "IPlayer.hpp"
#include <future>
#include <atomic>

namespace poker {

class HumanPlayer : public IPlayer {
public:
    HumanPlayer(PlayerId id, const std::string& name);

    PlayerId getId() const override { return m_id; };
    std::string getName() const override { return m_name; };
    void dealHoleCards(const Hand& cards) override;
    Action getAction(const PlayerView& view) override;

    void provideAction(Action action);

    bool isWaitingForInput() const { return m_waitingForInput.load(); }

private:
    PlayerId m_id;
    std::string m_name;
    Hand m_hand;

    std::promise<Action> m_promise;
    std::future<Action> m_future;
    std::atomic<bool> m_waitingForInput{false};

};

}