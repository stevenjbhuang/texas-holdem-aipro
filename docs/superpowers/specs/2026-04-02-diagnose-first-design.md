# Design: diagnose-first skill + CLAUDE.md complexity rule

**Date:** 2026-04-02
**Goal:** Reduce back-and-forth by forcing Claude to explore the codebase and diagnose root cause before proposing any solution on complex tasks.

---

## Problem

Claude often implements a narrow fix for the stated symptom without reading enough surrounding context or identifying the true root cause. This leads to iterative correction cycles where the user has to redirect Claude toward the underlying issue.

## Solution

Two coordinated pieces:

1. **CLAUDE.md addition** — defines what counts as "complex" and adds a hard self-trigger rule
2. **`diagnose-first` skill** — a four-stage structured checklist Claude runs before proposing anything

---

## CLAUDE.md Addition

A new "Task Complexity" section with:

### Complexity Threshold

- **Simple:** Single-site change where the cause is obvious without reading other files. Examples: formatting, typo, renaming, isolated off-by-one.
- **Complex:** Anything that touches multiple files, involves threading or shared state, requires understanding caller/callee relationships, or where the root cause is not immediately obvious from the stated symptom.

### Self-Trigger Rule

For complex tasks, Claude must self-invoke the `diagnose-first` skill before proposing any solution. This applies even when a task "seems obvious" — that is precisely when wrong-root-cause errors most often occur.

---

## The `diagnose-first` Skill

A rigid four-stage checklist. All stages are mandatory; none may be skipped.

### Stage 1 — Explore

Read the relevant files. Do not limit exploration to the file or function mentioned in the task. Specifically:
- Trace callers and callees of the affected code
- Follow data flow across files
- For this project: check which layer the change lives in, whether `GameState` or the mutex is involved, and whether the threading model is affected

### Stage 2 — Diagnose

State the root cause in one sentence before proceeding. If it cannot be stated clearly, exploration is incomplete. Distinguish:
- **Symptom:** what the user observed or reported
- **Root cause:** why it is happening in the code

### Stage 3 — Validate

Cross-check the diagnosis:
- Does the proposed root cause actually explain the observed behavior?
- Are there other call sites, files, or code paths that would also be affected?
- Does fixing this location fix the problem entirely, or does the same root cause manifest elsewhere?

### Stage 4 — Propose

Present the solution with:
- **What** changes (specific files and locations)
- **Why** this fixes the root cause (not just the symptom)
- **Tradeoffs considered** — name 1-2 alternatives that were ruled out and why, for any non-trivial change

---

## Skill File Location

The skill lives at: `.claude/skills/diagnose-first.md` (project-level).

It is self-invoked by Claude per the CLAUDE.md rule. It can also be invoked manually by the user.

---

## What This Does Not Change

- Simple tasks (formatting, typos, obvious single-site fixes) proceed without invoking the skill — keep these concise and direct.
- The skill does not replace brainstorming for new feature work. For new features, `superpowers:brainstorming` runs first, then `diagnose-first` is not needed.
- The existing `superpowers:systematic-debugging` skill is not replaced — it remains for debugging sessions. `diagnose-first` is broader and lighter-weight.

---

## Success Criteria

- Claude reads relevant surrounding code before proposing any fix on complex tasks
- Claude states the root cause explicitly before proposing a solution
- The number of "that doesn't address the real issue" corrections decreases
