#!/bin/bash
# doc-sync-check.sh
# Runs at SessionStart to detect if plan docs may be out of sync with recent code changes.
# Outputs additionalContext JSON so Claude asks the user once whether to consolidate docs.

# Uncommitted changes (staged or unstaged) in src/ or docs/plan/
DIRTY=$(git status --short -- src/ docs/plan/ 2>/dev/null | grep -v "^?" | wc -l | tr -d ' ')

# Recent commits that touched src/ files
RECENT_SRC=$(git log --oneline -10 --name-only 2>/dev/null | grep -c "^src/" 2>/dev/null || echo 0)

TOTAL=$((DIRTY + RECENT_SRC))

if [ "$TOTAL" -gt 0 ]; then
  MSG="DOCS SYNC REMINDER: Detected ${TOTAL} change indicator(s) in src/ or docs/plan/ (uncommitted edits or recent commits). At the very start of this session - before any other work - ask the user this question exactly once: 'I noticed recent changes to the project source or plan docs. Would you like me to review and consolidate the docs/plan/ phase files to reflect the current state of the codebase? (yes/no)' Wait for their answer before doing anything else. Do not ask again this session."
  printf '{"hookSpecificOutput":{"hookEventName":"SessionStart","additionalContext":"%s"}}\n' "$MSG"
fi
