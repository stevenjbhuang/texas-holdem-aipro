#include <gtest/gtest.h>
#include "players/AIPlayer.hpp"
#include "MockLLMClient.hpp"

using namespace poker;

static PlayerView makeTestView() {
    PlayerView v;
    v.myId   = 0;
    v.myHand = Hand{Card{Rank::Ace, Suit::Spades}, Card{Rank::King, Suit::Hearts}};
    v.pot    = 0;
    v.street = Street::PreFlop;
    v.chipCounts  = {{0, 200}};
    v.currentBets = {{0, 0}};
    v.legal.canCheck = true;
    return v;
}

TEST(AIPlayerTest, FoldResponse) {
    MockLLMClient mock;
    mock.responseToReturn = "FOLD";
    AIPlayer ai(0, "TestAI", mock, "");
    EXPECT_EQ(ai.getAction(makeTestView()).type, Action::Type::Fold);
}

TEST(AIPlayerTest, RaiseResponse) {
    MockLLMClient mock;
    mock.responseToReturn = "RAISE 200";
    AIPlayer ai(0, "TestAI", mock, "");
    Action a = ai.getAction(makeTestView());
    EXPECT_EQ(a.type,   Action::Type::Raise);
    EXPECT_EQ(a.amount, 200);
}

TEST(AIPlayerTest, RaiseLowercase) {
    MockLLMClient mock;
    mock.responseToReturn = "raise 50";
    AIPlayer ai(0, "TestAI", mock, "");
    Action a = ai.getAction(makeTestView());
    EXPECT_EQ(a.type,   Action::Type::Raise);
    EXPECT_EQ(a.amount, 50);
}

TEST(AIPlayerTest, GarbageFallback) {
    MockLLMClient mock;
    mock.responseToReturn = "I have no idea what to do here.";
    AIPlayer ai(0, "TestAI", mock, "");
    EXPECT_EQ(ai.getAction(makeTestView()).type, Action::Type::Call);
}
