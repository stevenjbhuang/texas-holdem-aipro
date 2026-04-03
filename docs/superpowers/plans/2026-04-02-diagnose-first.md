# diagnose-first Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a complexity threshold rule to CLAUDE.md and a `diagnose-first` skill file so Claude reads codebase context and diagnoses root cause before proposing solutions on complex tasks.

**Architecture:** CLAUDE.md defines what counts as "simple" vs "complex" and instructs Claude to read `.claude/skills/diagnose-first.md` before acting on complex tasks. The skill file contains the four-stage checklist (Explore → Diagnose → Validate → Propose). No plugin registration required — Claude reads the file directly via the Read tool.

**Tech Stack:** Markdown only.

---

### Task 1: Add Task Complexity section to CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Read the current CLAUDE.md**

Open `CLAUDE.md` and identify where to insert the new section. Insert it after the "Doing tasks" / "Tone and style" / code style sections — before "Key Files". The goal is to have it loaded early and unambiguously.

- [ ] **Step 2: Add the Task Complexity section**

Insert the following block into `CLAUDE.md` after the "Code Style" section and before "Key Files":

```markdown
## Task Complexity

### Threshold

Before acting on any task, classify it:

- **Simple:** Single-site change where the cause is obvious without reading other files. Examples: formatting, typo, renaming, isolated off-by-one with a clear fix.
- **Complex:** Anything that touches multiple files, involves threading or shared state, requires understanding caller/callee relationships, or where the root cause is not immediately obvious from the stated symptom.

### Self-Trigger Rule

For **complex** tasks: before proposing any solution, read `.claude/skills/diagnose-first.md` and follow the four-stage checklist it defines. Do not skip this even when a task "seems obvious" — that is precisely when wrong-root-cause errors most often occur.

For **simple** tasks: proceed directly, keep the response concise.
```

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "feat(claude): add task complexity threshold and diagnose-first self-trigger rule"
```

---

### Task 2: Create the diagnose-first skill file

**Files:**
- Create: `.claude/skills/diagnose-first.md`

- [ ] **Step 1: Create the `.claude/skills/` directory and skill file**

Create `.claude/skills/diagnose-first.md` with the following content:

```markdown
# diagnose-first

A four-stage checklist for complex tasks. All stages are mandatory. Do not skip any stage, even if the answer seems obvious after the previous stage.

---

## Stage 1 — Explore

Read the relevant files. Do not limit exploration to the file or function mentioned in the task description.

Checklist:
- Identify which layer(s) the change lives in (`core/`, `ui/`, `players/`, `ai/`)
- Read the file(s) mentioned in the task
- Trace callers: who calls the affected function/method?
- Trace callees: what does the affected code call?
- Follow data flow: where does the relevant data come from, and where does it go?
- Check threading: is `GameState`, `m_stateMutex`, `waitingForAction`, or a `std::promise`/`std::future` involved?
- Check layer boundaries: does the proposed change respect the no-cross-layer-dependency rule?

Do not proceed to Stage 2 until you have read all files whose behavior is relevant to the task.

---

## Stage 2 — Diagnose

State the root cause in one sentence before proceeding.

Format:
> **Symptom:** [what the user observed or reported]
> **Root cause:** [why it is happening in the code — specific file, function, or logic]

If you cannot state the root cause clearly in one sentence, return to Stage 1 — exploration is incomplete.

---

## Stage 3 — Validate

Cross-check the diagnosis against the codebase:

- Does the proposed root cause actually explain the observed behavior end-to-end?
- Are there other call sites, files, or code paths with the same root cause that would also need to change?
- If you fix only the location you identified, does the problem go away entirely — or does the same root cause manifest elsewhere?

State your validation result explicitly:
> **Validation:** [confirmed / partially confirmed — also affects X / not confirmed — returning to Stage 1]

---

## Stage 4 — Propose

Present the solution with:

1. **What changes** — specific files and locations (file:line where possible)
2. **Why this fixes the root cause** — not just the symptom; connect the fix to the Stage 2 diagnosis
3. **Alternatives considered** — for any non-trivial change, name 1-2 alternatives that were ruled out and why

Do not present code until Stage 4. Presenting code before diagnosing the root cause skips the process and defeats its purpose.
```

- [ ] **Step 2: Commit**

```bash
git add .claude/skills/diagnose-first.md
git commit -m "feat(skills): add diagnose-first four-stage checklist for complex tasks"
```
