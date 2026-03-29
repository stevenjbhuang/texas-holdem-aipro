#include <gtest/gtest.h>
#include "ai/PromptBuilder.hpp"
#include "core/PlayerView.hpp"

using namespace poker;

// Helper: build a minimal two-player view for player 0
static PlayerView makeView() {
    PlayerView v;
    v.myId   = 0;
    v.myHand = Hand{Card{Rank::Ace, Suit::Spades}, Card{Rank::King, Suit::Hearts}};
    // Opponent (player 1) holds Two of Clubs, Three of Diamonds — never appears in view
    v.pot    = 20;
    v.street = Street::Flop;
    v.communityCards = {
        Card{Rank::Jack, Suit::Spades},
        Card{Rank::Ten,  Suit::Hearts},
        Card{Rank::Nine, Suit::Diamonds},
    };
    v.chipCounts  = {{0, 180}, {1, 200}};
    v.currentBets = {{0, 0},  {1, 10}};  // player 1 bet $10; player 0 must call
    v.minRaise    = 10;
    v.dealerButton   = 0;
    v.smallBlindSeat = 0;
    v.bigBlindSeat   = 1;

    v.legal.canCall    = true;
    v.legal.callCost   = 10;
    v.legal.canRaise   = true;
    v.legal.minRaiseTo = 20;
    v.legal.maxRaiseTo = 180;

    return v;
}

TEST(PromptBuilderTest, ContainsPersonalityText) {
    auto prompt = PromptBuilder::build(makeView(), "BE AGGRESSIVE");
    EXPECT_NE(prompt.find("BE AGGRESSIVE"), std::string::npos);
}

TEST(PromptBuilderTest, ContainsOwnHoleCards) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("As"), std::string::npos);  // Ace of Spades
    EXPECT_NE(prompt.find("Kh"), std::string::npos);  // King of Hearts
}

TEST(PromptBuilderTest, ContainsCommunityCards) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("Js"), std::string::npos);  // Jack of Spades
    EXPECT_NE(prompt.find("Th"), std::string::npos);  // Ten of Hearts
    EXPECT_NE(prompt.find("9d"), std::string::npos);  // Nine of Diamonds
}

TEST(PromptBuilderTest, PreFlopShowsNoneCommunity) {
    auto v = makeView();
    v.street = Street::PreFlop;
    v.communityCards.clear();
    auto prompt = PromptBuilder::build(v, "");
    EXPECT_NE(prompt.find("none"), std::string::npos);
}

TEST(PromptBuilderTest, ContainsFoldAndCall) {
    auto prompt = PromptBuilder::build(makeView(), "");
    EXPECT_NE(prompt.find("FOLD"), std::string::npos);
    EXPECT_NE(prompt.find("CALL"), std::string::npos);
}

TEST(PromptBuilderTest, RaiseAbsentWhenCannotAfford) {
    auto v = makeView();
    v.chipCounts[0]  = 5;   // player 0 only has $5 left
    v.currentBets[0] = 10;
    v.currentBets[1] = 10;
    v.minRaise = 100;       // minimum raise would cost $100 — impossible
    v.legal.canRaise = false;
    auto prompt = PromptBuilder::build(v, "");
    EXPECT_EQ(prompt.find("RAISE"), std::string::npos);
}