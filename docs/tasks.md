# Task plan

This file is the hand-off point between the agents in `.claude/agents/`:

```
docs/spec.md ──► planner ──► docs/tasks.md ──► implementer (one task) ──► reviewer ──► merge
                  (read-only)  (this file)       writes code, runs gates    re-runs gates, verdict
```

1. **Spec.** Write the requirement and its testable acceptance criteria in `docs/spec.md`.
2. **Plan.** Run the `planner` agent. It is read-only, so the caller (you or the main Claude session) pastes its full output under **Current plan** below, replacing the previous plan.
3. **Approve.** For every task with `needs_human_approval: true`, a human adds an `approved:` line to the task block (see the conventions below). The implementer refuses unapproved tasks.
4. **Implement.** Run the `implementer` agent with one task ID, e.g. "Implement T3 from docs/tasks.md". Work in dependency order; `parallel_safe` tasks may run together.
5. **Review.**
   - Run the `reviewer` agent with the same task ID and the implementer's report.
   - `pass`: set the task's `status: done` and record the result in the **Progress log**.
   - `fix`: send the reviewer's instructions back to the implementer, then review again (round 2, 3, ...).
   - `escalate`: a human or the planner decides, and the task becomes `blocked` until then.
6. **Finish.** When every task is `done`, run `/build`. Then do the **Definition of done** checks, including the hardware steps only a human can perform.

## Conventions

- Task blocks start with `### T<number>: <title>`, numbered from **T1**. Do not reuse numbers; if the plan is replaced, record what happened to the old tasks in the Progress log.
- `status` is one of:
  - `ready`: can start;
  - `blocked`: waiting on an open question, a dependency or approval;
  - `done`: the reviewer passed it.
  Only the caller changes status; the agents only read it.
- To approve, the human adds this line directly under `needs_human_approval: true` in the task block:
  `- approved: <who>, <YYYY-MM-DD>, <scope of approval>`
  The caller must also state the approval when invoking the implementer.
- `hardware_verification` steps (flashing, bench tests, in-car checks) are done by a human only. Record the outcome in the Progress log before the task counts as fully verified.
- Gate commands are defined in `.claude/agents/*.md` and `.claude/commands/build.md`. Tasks should quote them exactly:
  - `./scripts/test_host.sh`
  - `idf.py build` with zero `main/` warnings
  - the demo build
- The task format is defined in `.claude/agents/planner.md`. Keep this file parseable: no task-like `### T` headings outside **Current plan**.

---

## Current plan

_No plan yet. `docs/spec.md` is empty, so the planner will stop until it has a requirement with acceptance criteria._

<!--
Paste the planner's output here, in its section order:
1. Summary
2. Acceptance criteria
3. Assumptions
4. Open questions
5. Tasks
6. Execution order
7. Risks
8. Out of scope / follow-ups
9. Definition of done
-->

---

## Progress log

Newest last. One row per implementer run, review round, approval, escalation or hardware check.

| Date | Task | Event | Result | Notes |
|------|------|-------|--------|-------|
