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
   - Review the draft and answer its open questions. Then say "make CR-<n> active". The caller records your answers and any approvals, and sets `Status: active` in both places.
2. **Plan (starts automatically).**
   - Making a CR active is the go-ahead to plan it, so the caller runs the `planner` agent straight away. No separate "plan" request is needed.
   - The planner is read-only. The caller pastes its full output under **Current plan** in that CR's `tasks.md`, then **stops** so you can review the plan before any code is written.
   - Say "go" (or ask for changes) to start implementing.
3. **Approve.** For every task with `needs_human_approval: true`, a human adds an `approved:` line to the task block (see the conventions below). The implementer refuses unapproved tasks.
4. **Implement.**
   - Note the task base commit (`git rev-parse HEAD`).
   - Run the `implementer` agent with one task, e.g. "Implement CR-2 T3 from docs/changes/CR-2/tasks.md".
   - Work in dependency order; `parallel_safe` tasks may run together.
   - **Commit its output** as `CR-<n> T<m> round 1: <summary>`. The implementer never commits. The commit gives the reviewer a fixed state to review and to diff against in the next round.
5. **Review loop.** See [Review rounds](#review-rounds) for exactly what to pass each agent and what to record.
   - Run the `reviewer` agent for round 1.
   - `pass`:
     1. Mark any minors or nits still open as `accepted` or `follow-up` in the Findings table.
     2. Set the task's `status: done`.
     3. Log the result in the Progress log and commit.
   - `fix`:
     1. Record the findings.
     2. Re-run the implementer on every open blocker and major, plus any minors or nits you choose to fix. Those you don't choose are marked `accepted` or `follow-up` at pass.
     3. Commit the round.
     4. Run the reviewer for the next round.
   - `escalate`:
     - A human or the planner decides, and the task is `blocked` until then.
     - The decision is recorded on the finding (`accepted`, `follow-up`, or a criterion amendment).
     - If an acceptance criterion changes, edit `request.md` with an *Amended* note, log it, and continue the loop.
6. **Finish.**
   - When every task is `done`, run `/build`. Then do the **Definition of done** checks, including the hardware steps only a human can perform.
   - Set `Status: done` in `request.md` and in the `docs/spec.md` §3 index.
   - If the CR amended any system requirement, update its wording in `docs/spec.md` §2.
   - Open the PR with `Closes #<issue>`, so merging closes the change-request issue.
   - **Wait for CI to pass before merging.** The `CI` workflow (`.github/workflows/ci.yml`) runs the same gates as `/build` on GitHub: host tests, plus the normal and demo firmware builds with no `main/` warnings. A red CI run blocks the merge, even if `/build` passed locally.
   - Tick the issue's acceptance-criteria checkboxes, and paste the reviewer's verdicts into the PR as a comment.
   - If hardware steps are still owed, keep a `hardware-test` label on the issue (or a follow-up issue) until you record the results.

## Review rounds

The review loop keeps its state in the CR's `tasks.md`, not in anyone's memory. Any session can therefore resume it. The caller (you or the main Claude session) does the bookkeeping, and the agents only read it.

**Findings table.** It lives in `tasks.md` under **Findings**, with one row per finding:

| Column | Content |
|--------|---------|
| ID | `T<m>-F<k>`: numbered per task by the reviewer, never reused |
| Task | `T<m>` |
| Raised | the round it was raised in |
| Severity | blocker, major, minor or nit |
| Title | the reviewer's short title |
| Status | `open`, `fixed`, `disputed`, `withdrawn`, `escalated`, `accepted` (left as is by decision) or `follow-up` (moved to a later CR or issue) |
| Closed | the round, or the decision date |
| Notes | the evidence or the decision |

**What each agent gets:**

| Round | Implementer gets | Reviewer gets |
|-------|------------------|---------------|
| 1 | the task ID | task ID, round 1, task base commit, the implementer's report |
| ≥2 | task ID, round number, the findings to fix (ID, severity, direction): every open blocker and major, plus any minors or nits chosen | task ID, round number, task base commit, previous review commit, the open findings rows, the implementer's report with its **Response to findings** |

**After each review, the caller:**
1. adds new findings to the table, with the reviewer's IDs;
2. updates the status of each prior finding from the reviewer's **Prior findings** verdicts: `fixed`, `open`, `withdrawn`, or `escalated` when a dispute is rejected;
3. adds a Progress log row: round, verdict, and finding IDs raised or closed.

**Rules both agents follow:**
- From round 2, the reviewer checks the open findings against the fix diff (`<previous review commit>..HEAD`) and re-runs the gates.
  - It raises a new finding only if the fix introduced it, or if it is a blocker.
  - It never re-raises an `accepted`, `follow-up` or `withdrawn` finding.
- The implementer answers every finding it was given with `fixed`, `disputed` (with evidence, and no change) or `needs-scope` (no change, and it stops).
  - It changes nothing else. Unavoidable side effects are listed as **Extras**.
- **Automatic escalation:**
  - a finding still `open` two fix rounds after it was raised;
  - a `needs-scope` response;
  - a dispute the reviewer rejects.

## Slack notifications

The main session posts short **status-only** messages to Slack at each lifecycle point using `.claude/hooks/slack-notify.sh`. Every message names the CR, because task numbers restart at T1 in each change request. The agents never post. No agent output, diff, finding text or error text is ever sent: only IDs, verdicts, counts and short codes.

| When (caller) | Command | Needs you? |
|---|---|---|
| Plan saved to `tasks.md`, stopping for review | `plan-ready cr=CR-<n> tasks=<k>` | yes: review the plan, then "go" |
| A `needs_human_approval` task is next and has no `approved:` line | `approval-needed cr=CR-<n> task=T<m>` | yes |
| Implementer started on a task (any round) | `task-start cr=CR-<n> task=T<m> round=<r>` | no |
| Implementer reported, round committed | `gates cr=CR-<n> task=T<m> host=pass build=pass demo=skip` | no |
| Reviewer returned a verdict | `review cr=CR-<n> task=T<m> round=<r> verdict=pass\|fix\|escalate blockers=<b> majors=<j>` | no |
| Reviewer escalated | `escalation cr=CR-<n> task=T<m> reason=<short-code>` | yes: decide |
| Task marked `done` | `task-done cr=CR-<n> task=T<m> rounds=<r>` | no |
| PR opened / CI finished | `pr-ready cr=CR-<n> pr=#<p> ci=pending\|pass\|fail` | yes when `ci=pass`: merge |
| Hardware follow-up issue opened | `bench-owed cr=CR-<n> issue=#<i> step=<short-code>` | yes, at the bench |
| PR merged | `cr-done cr=CR-<n> tasks=<k> findings=<f> pr=#<p>` | no |

Notes:
- **Short codes.** `reason` and `step` are short kebab-case codes, such as `criterion-gap`, `needs-scope`, `dispute-rejected` or `below-range-hold`. They are never free text.
- **Idle prompts.** `.claude/settings.json` registers a **Notification** hook (`needs-attention`) for idle, permission and question prompts, so you're also told when the session is simply waiting for you.
  - There is deliberately **no Stop hook**. Stop fires at the end of every turn, including turns that end while background agents are still working, so it would send false "waiting for you" messages.
  - To opt in for yourself anyway, set `NOTIFY_STOP=1` and add a Stop hook to your own `.claude/settings.local.json`.
- **Setup and testing:**
  - **Setup:** the webhook URL goes in `~/.config/claude-slack-notify/webhook`, shared by all your projects, or in `~/.config/claude-slack-notify/Caterham-Oil-Temperature-Gauge.webhook` for a channel of its own. `chmod 600` it, and never put it in the repo.
  - **Reuse:** the script has no project-specific names. To reuse it, copy `.claude/hooks/slack-notify.sh` and the `hooks` and `deny` entries from `.claude/settings.json` into another repo. Messages are labelled with that repo's folder name.
  - **Preview without posting:** `NOTIFY_DRY_RUN=1 .claude/hooks/slack-notify.sh <type> ...`.
  - **No webhook file:** nothing is sent. The script logs and exits 0, so a missing or broken Slack setup never blocks the workflow.

## Conventions

- Task blocks start with `### T<number>: <title>`, numbered from **T1** within each change request. Refer to a task as `CR-<n> T<m>`. Do not reuse numbers; if the plan is replaced, record what happened to the old tasks in the Progress log.
- `status` is one of:
  - `ready`: can start;
  - `blocked`: waiting on an open question, a dependency or approval;
  - `done`: the reviewer passed it.
  Only the caller changes task and finding statuses; the agents only read them.
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
