# Phase 9 — Polish & AI Tuning

**This phase is open-ended — explore at your own pace.**

**Previous phase:** [Phase 8 — Wire It All Together](phase-8-wire-up.md)

---

### Task 9.1: Tune the AI prompt

- [ ] Play a few hands. Does the AI make sensible decisions?
- [ ] Iterate on `config/personalities/default.md` and `PromptBuilder.cpp`
- [ ] Try different Ollama models: `llama3.2`, `mistral`, `phi3`

---

### Task 9.2: Add personality variety

- [ ] Assign different personality files to each AI player
- [ ] Add a new personality (e.g. `maniac.md` — raises every hand)
- [ ] Observe how different personalities affect the game

---

### Task 9.3: Improve the renderer

- [ ] Add real card sprites (download a free card asset set to `assets/cards/`)
- [ ] Show action history log on screen
- [ ] Animate chip movements

---

### Task 9.4: Add a raise input widget

- [ ] Add a slider or text input for the raise amount
- [ ] Clamp to `[minRaise, playerStack]`
