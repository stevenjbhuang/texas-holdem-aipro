# Texas Hold'em Rules — Developer Reference

This document describes the specific rules variant implemented in this project. Use it when implementing or debugging `GameEngine`, writing tests, or verifying edge-case behaviour.

---

## Overview

Texas Hold'em is a community card poker game. Each player receives two private hole cards and shares five community cards. The best five-card hand from any combination of the seven cards wins the pot.

---

## Game Flow

```
Start of hand
  │
  ├─ Rotate dealer button
  ├─ Post blinds (SB + BB)
  ├─ Deal 2 hole cards to each active player
  │
  ├─ Pre-Flop betting round   (UTG acts first)
  ├─ Deal 3 community cards (Flop)
  ├─ Flop betting round       (first active player left of dealer acts first)
  ├─ Deal 1 community card (Turn)
  ├─ Turn betting round
  ├─ Deal 1 community card (River)
  ├─ River betting round
  │
  └─ Showdown → award pot → next hand
```

Early exit: if all players but one fold at any point, the remaining player wins the pot immediately without a showdown.

---

## Positions

### 3+ players
| Position | Seat |
|---|---|
| Dealer (button) | rotates clockwise each hand |
| Small blind (SB) | 1 seat left of dealer |
| Big blind (BB) | 2 seats left of dealer |
| UTG (under the gun) | 3 seats left of dealer — acts first pre-flop |

### Heads-up (2 players)
| Position | Seat |
|---|---|
| Dealer / Small blind | same player |
| Big blind | the other player |

The dealer posts the small blind and acts *first* pre-flop but *last* post-flop. This is the standard heads-up rule.

### Eliminated players
When a player is eliminated (chips = 0), the dealer button and blind assignments skip them. `rotateDealerButton()` walks forward to the next active player; SB and BB are assigned using the same skip logic.

---

## Blinds

Blinds are forced bets posted before any cards are dealt.

- **Small blind:** `config.smallBlind` chips
- **Big blind:** `config.bigBlind` chips (typically 2× small blind)

If a player cannot cover the full blind they post what they have (all-in). The blind counts toward their `currentBets` and `totalContributed` for the hand.

---

## Betting Rounds

### Action order
- **Pre-flop:** UTG acts first, continues clockwise, ends with the BB.
- **Post-flop:** first active (non-folded, non-bust) player left of the dealer acts first.

### Legal actions
| Action | When legal |
|---|---|
| **Fold** | always |
| **Check** | when no bet has been made this street (call cost = 0) |
| **Call** | match the current highest bet; cost = `maxBet − yourCurrentBet` |
| **Raise** | increase the bet; total must be at least `maxBet + minRaise`; max is your remaining stack (all-in) |

### Minimum raise
`minRaise` is the minimum *increase* above the current bet, not the total. It starts at `bigBlind` and is updated to the size of the last raise:

```
minRaise = lastRaiseTotal − previousMaxBet
```

Example: BB is $10. UTG raises to $30 (raise of $20). `minRaise` becomes $20. The next raise must be to at least $50.

### Round end
The betting round ends when `needsToAct` is empty — every player has either folded, called the current bet, or gone all-in. A raise restarts the round: all other active players must act again.

### BB option
The big blind acts last pre-flop. If no one raised, the BB may raise (their "option") even though they already posted the big blind. This is enforced because the BB is included in `actionOrder` and `needsToAct`.

---

## All-in

A player who commits all their chips is *all-in*. They remain eligible to win up to the amount they contributed per player (see Side Pots). They are excluded from further betting (removed from `needsToAct` and future `actionOrder`) but are *not* added to `foldedPlayers`.

---

## Side Pots

Side pots are created when one or more players are all-in for different amounts.

**Algorithm (`determineWinner`):**
1. Collect all distinct `totalContributed` levels across players.
2. For each level, compute the pot slice: `sum of min(contribution, level) − min(contribution, prevLevel)` across all players.
3. Eligible players for each slice: non-folded players who contributed at least `level`.
4. Award each slice to the best eligible hand. If only one eligible player, award without evaluation.

**Example:**
- Player A: all-in for $50 → eligible for pot up to $50/player
- Player B: all-in for $100
- Player C: active with $200

Three side pots are created at levels $50, $100, and whatever C committed beyond $100.

---

## Showdown

If two or more non-folded players remain after the River betting round, hands are evaluated. The `HandEvaluator` scores each 7-card combination (2 hole + 5 community); lower score = better hand. The player with the lowest score wins.

**Ties:** if two or more players have equal scores, the pot is split equally. Any indivisible remainder goes to the first eligible player in seat order (standard house rule).

**No community cards:** if the hand ends before the River (all-but-one fold), the winner takes the pot without evaluation.

---

## Hand Rankings (best to worst)

| Rank | Name | Example |
|---|---|---|
| 1 | Royal Flush | A K Q J T (same suit) |
| 2 | Straight Flush | 9 8 7 6 5 (same suit) |
| 3 | Four of a Kind | A A A A x |
| 4 | Full House | A A A K K |
| 5 | Flush | A J 9 6 2 (same suit) |
| 6 | Straight | 9 8 7 6 5 (mixed suits) |
| 7 | Three of a Kind | A A A x y |
| 8 | Two Pair | A A K K x |
| 9 | One Pair | A A x y z |
| 10 | High Card | A x y z w (no combination) |

The evaluator finds the best five-card hand from all seven available cards automatically.

---

## Stack and Chip Accounting

**Invariant:** at all times, `sum(chipCounts) + pot == numPlayers × startingStack`.

- `chipCounts[id]`: current stack (chips not yet in the pot)
- `currentBets[id]`: chips committed this street (still counts toward pot)
- `totalContributed[id]`: chips committed this entire hand (used for side pot calculation)
- `pot`: total chips in play this hand

`currentBets` is reset to 0 at the start of each new street. `totalContributed` is reset at the start of each new hand.

---

## Edge Cases Implemented

| Scenario | Behaviour |
|---|---|
| Player cannot cover the blind | Posts remaining chips; goes all-in |
| Only one active player remains mid-hand | `awardPotIfHandOver()` triggers early exit — no showdown |
| All remaining players are all-in | Board is dealt to completion; `needsToAct` is empty so betting skips automatically |
| Contested pot with < 7 cards (future path) | Pot is split equally among eligible players (chip conservation fallback) |
| SB/BB seat is a bust player | `rotateDealerButton()` skips bust players when assigning SB and BB |
