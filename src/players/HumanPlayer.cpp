#include "HumanPlayer.hpp"

namespace poker {

HumanPlayer::HumanPlayer(PlayerId id, const std::string& name)
    : m_id(id), m_name(name) {}

void HumanPlayer::dealHoleCards(const Hand& cards) {
    m_hand = cards;
}

Action HumanPlayer::getAction(const PlayerView& view) {
    m_promise = std::promise<Action> {};
    m_future = m_promise.get_future();

    m_waitingForInput = true;
    Action action = m_future.get();
    m_waitingForInput = false;

    return action;
}

void HumanPlayer::provideAction(Action action) {
    m_promise.set_value(action);
}

}