# Change requests

Each change request (CR) has its own folder. The folder is the complete, permanent record of that change:

```
docs/spec.md                  system requirements: how the gauge behaves now (§3 indexes the CRs)
docs/changes/
  README.md                   this file: workflow and conventions
  _template/                  copy for a new CR (/spec-from-issue does this)
    request.md
    tasks.md
  CR-<n>/
    request.md                what to change: motivation, scope, acceptance criteria, status
    tasks.md                  how it was done: the planner's plan and the progress log
```

`request.md` carries the status: `draft` → `active` → `done` (or `withdrawn`). At most one CR is `active` at a time. Folders are never deleted. Once a CR is `done` or `withdrawn`, its folder is a read-only record.

## Workflow

```
issue ──► /spec-from-issue ──► CR-<n>/request.md ──► planner ──► CR-<n>/tasks.md ──► implementer ──► reviewer ──► /build ──► PR "Closes #<issue>"
           (drafts, you review)                       (read-only)                    (one task)       (verdict)
```

1. **Request.**
   - Each CR starts as a GitHub issue labelled `change-request`.
   - Run `/spec-from-issue <n>`. It creates `docs/changes/CR-<n>/` from `_template/`, drafts `request.md` with `Status: draft`, and adds the CR to the index in `docs/spec.md` §3.
   - Review the draft, answer its open questions, and set `Status: active` in both places.
2. **Plan.** Run the `planner` agent on the active CR. It is read-only, so the caller (you or the main Claude session) pastes its full output under **Current plan** in that CR's `tasks.md`.
3. **Approve.** For every task with `needs_human_approval: true`, a human adds an `approved:` line to the task block (see the conventions below). The implementer refuses unapproved tasks.
4. **Implement.** Run the `implementer` agent with one task, e.g. "Implement CR-2 T3 from docs/changes/CR-2/tasks.md". Work in dependency order; `parallel_safe` tasks may run together.
5. **Review.**
   - Run the `reviewer` agent with the same task and the implementer's report.
   - `pass`: set the task's `status: done` and record the result in the **Progress log**.
   - `fix`: send the reviewer's instructions back to the implementer, then review again (round 2, 3, ...).
   - `escalate`: a human or the planner decides, and the task becomes `blocked` until then. If the decision amends an acceptance criterion, edit `request.md` with an *Amended* note and log it.
6. **Finish.**
   - When every task is `done`, run `/build`. Then do the **Definition of done** checks, including the hardware steps only a human can perform.
   - Set `Status: done` in `request.md` and in the `docs/spec.md` §3 index.
   - If the CR amended any system requirement, update its wording in `docs/spec.md` §2.
   - Open the PR with `Closes #<issue>`, so merging closes the change-request issue.
   - Tick the issue's acceptance-criteria checkboxes, and paste the reviewer's verdicts into the PR as a comment.
   - If hardware steps are still owed, keep a `hardware-test` label on the issue (or a follow-up issue) until you record the results.

## Conventions

- Task blocks start with `### T<number>: <title>`, numbered from **T1** within each change request. Refer to a task as `CR-<n> T<m>`. Do not reuse numbers; if the plan is replaced, record what happened to the old tasks in the Progress log.
- `status` is one of:
  - `ready`: can start;
  - `blocked`: waiting on an open question, a dependency or approval;
  - `done`: the reviewer passed it.
  Only the caller changes status; the agents only read it.
- To approve, the human adds this line directly under `needs_human_approval: true` in the task block:
  `- approved: <who>, <YYYY-MM-DD>, <scope of approval>`
  The caller must also state the approval when invoking the implementer.
- GitHub labels mirror these states:
  - `blocked` on the issue while a question, dependency or approval is pending;
  - `needs-approval` while a `needs_human_approval` task has no `approved:` line;
  - `hardware-test` while bench or in-car checks are owed.
  The files stay authoritative, and the labels are the public view.
- `hardware_verification` steps (flashing, bench tests, in-car checks) are done by a human only. Record the outcome in the Progress log before the task counts as fully verified.
- Gate commands are defined in `.claude/agents/*.md` and `.claude/commands/build.md`. Tasks should quote them exactly:
  - `./scripts/test_host.sh`
  - `idf.py build` with zero `main/` warnings
  - the demo build
- The task format is defined in `.claude/agents/planner.md`. Keep each `tasks.md` parseable: no task-like `### T` headings outside **Current plan**.
