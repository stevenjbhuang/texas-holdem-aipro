Verify my current phase implementation by running the phase-verifier agent.

Use the Agent tool to launch the `phase-verifier` subagent. It will:
1. Detect which phase is active from the source files present
2. Configure and build the project
3. Run tests if applicable
4. Spot-check the code for common mistakes from the phase plan
5. Return a structured report with pass/fail results and educational feedback
