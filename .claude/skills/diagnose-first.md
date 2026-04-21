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
