# Change requests

Each folder here, `CR-<n>/`, is the complete, permanent record of one change request:
- `request.md`: what to change, with its acceptance criteria, approvals and status;
- `tasks.md`: how it was done, with the plan, the review findings and the progress log.

The workflow is run by the **cr-workflow** Claude Code plugin. Its full description (lifecycle, review rounds, bookkeeping, conventions and Slack notifications) is in the plugin's `docs/workflow.md`. Quick reference:

| Step | Command |
|---|---|
| Turn an issue into a draft CR | `/cr-workflow:spec-from-issue <issue>` |
| Approve the draft and start planning | say "make CR-<n> active" (or `/cr-workflow:cr activate CR-<n>`) |
| Implement and review every task | `/cr-workflow:cr go` |
| Where are we? | `/cr-workflow:cr status` |
| Run the gates | `/cr-workflow:build` |
| Open the PR / merge on green CI | `/cr-workflow:cr pr CR-<n>`, then `/cr-workflow:cr merge CR-<n>` |

The project-specific rules (gates, invariants, approvals, human-only checks) are in `CLAUDE.md`.
