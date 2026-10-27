# Task plan

This file is the hand-off point between the agents in `.claude/agents/`:

```
docs/spec.md ──► planner ──► docs/tasks.md ──► implementer (one task) ──► reviewer ──► merge
                  (read-only)  (this file)       writes code, runs gates    re-runs gates, verdict
```

1. **Spec.** Each change request starts as a GitHub issue labelled `change-request`. Run `/spec-from-issue <n>` to draft it as the change request in `docs/spec.md`. Review the draft and set `Status: active`.
2. **Plan.** Run the `planner` agent. It is read-only, so the caller (you or the main Claude session) pastes its full output under **Current plan** below, replacing the previous plan.
3. **Approve.** For every task with `needs_human_approval: true`, a human adds an `approved:` line to the task block (see the conventions below). The implementer refuses unapproved tasks.
4. **Implement.** Run the `implementer` agent with one task ID, e.g. "Implement T3 from docs/tasks.md". Work in dependency order; `parallel_safe` tasks may run together.
5. **Review.**
   - Run the `reviewer` agent with the same task ID and the implementer's report.
   - `pass`: set the task's `status: done` and record the result in the **Progress log**.
   - `fix`: send the reviewer's instructions back to the implementer, then review again (round 2, 3, ...).
   - `escalate`: a human or the planner decides, and the task becomes `blocked` until then.
6. **Finish.**
   - When every task is `done`, run `/build`. Then do the **Definition of done** checks, including the hardware steps only a human can perform.
   - Open the PR with `Closes #<issue>`, so merging closes the change-request issue.
   - Tick the issue's acceptance-criteria checkboxes, and paste the reviewer's verdicts into the PR as a comment.
   - If hardware steps are still owed, keep a `hardware-test` label on the issue (or a follow-up issue) until you record the results.

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
- The task format is defined in `.claude/agents/planner.md`. Keep this file parseable: no task-like `### T` headings outside **Current plan**.

---

## Current plan

### Plan for CR-1 (issue #5): Close host-test coverage gaps in the alarm and conversion logic

#### 1. Summary
CR-1 adds characterisation host tests for:
- alarm debounce and the separate escalation timers;
- millisecond wraparound;
- the latched alarm through a brief fault;
- the Cold status;
- the calibration table endpoints;
- max tracking;
- `statusText`.

The tests are expected to pass against the current `main/core/` code. The reviewer's mutation check proves they are strong enough.

There are four `test` tasks and no `impl` tasks:
- T1–T3 edit `tests/demo_temperature_test.cpp` (built with `-DGAUGE_DEMO`).
- T4 edits `tests/temperature_test.cpp`.
- Each task is verified with `./scripts/test_host.sh` plus a `git diff --stat` scope check.

#### 2. Acceptance criteria → tasks
| AC | Tasks |
|----|-------|
| AC-1, AC-2, AC-3 | T1 |
| AC-4, AC-5 | T2 |
| AC-6, AC-7, AC-8 | T3 |
| AC-9 | T4 |
| AC-10 | T1–T4 (each task's verification), Definition of done |

All criteria can be tested on the host. No bench checks.

#### 3. Assumptions
- No `CLAUDE.md` exists. The rules come from the agent prompts and `docs/spec.md`.
- CR-1 explicitly overrides the usual "test-first fails" rule. These tests must pass against the current code, so there are no `impl` tasks.
- If an assertion fails, follow the CR-1 defect rule: leave it out, record the evidence and escalate. Never change `main/`.
- AC-9 goes in `tests/temperature_test.cpp`, because `statusText` doesn't depend on calibration.
- AC-4 is a single `TemperatureState` sequence. It starts at `0xFFFFF000`, reproduces AC-2, holds Red, then runs a sustained open-circuit fault whose `kFaultResetMs` window crosses the wrap.

#### 4. Open questions
None blocking.

#### 5. Tasks

### T1: Add debounce and escalation-timer characterisation tests (AC-1, AC-2, AC-3)
- kind: test
- status: done
- depends_on: []
- parallel_safe: false
- needs_human_approval: false
- goal: Pin down the exact Amber and Red debounce timing and the separate escalation timers, so a change to the `>=` comparison or a shared timer in `Temperature.cpp` fails the host tests.
- files_may_touch:
  - tests/demo_temperature_test.cpp
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/test_calibration_tool.py
  - main/**
  - scripts/**
  - hardware/**
  - docs/**
  - build/
  - managed_components/
  - sdkconfig
- acceptance_criteria:
  - **AC-1 block.** Use a fresh `TemperatureState` and step `at(120)` every 250 ms from 0.
    - At every update with ms < 2500: `alarm == Status::Normal`.
    - At every update from 2500 ms to at least 5000 ms: `alarm == Status::Amber` and `status == Status::Amber`.
  - **AC-2 block.** Use a fresh state and step `at(139)` every 250 ms from 0.
    - At 2250: `alarm == Normal`.
    - At 2500: `alarm == Red`.
    - Assert at these exact timestamps, not only at the end of a `hold`.
  - **AC-3 block.**
    1. Use a fresh state, latch Amber with `at(120)` (for example `hold` for 4000 ms), and assert `alarm == Amber`.
    2. Step to a sustained `at(139)`. Record `t0`, the ms of the first update with `has_temperature && temperature_c >= 130`.
    3. Assert `alarm == Amber` at every update before `t0 + 2500`.
    4. Assert `alarm == Red` at every update from `t0 + 2500` to at least `t0 + 4000`.
    5. Assert that `t0` was actually found.
  - Each AC is its own `{ ... }` block with a comment naming the AC, in the existing test style.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites print their "passed" lines; the Python unittests report OK
  - `git diff --stat` -> only `tests/demo_temperature_test.cpp`
- hardware_verification: none
- notes:
  - Reuse `forOhms`, `at`, `hold` and `kOpen` (tests/demo_temperature_test.cpp:10-26). Don't change `hold`'s signature.
  - Per-update assertions need an explicit loop over `s.update(x, ms)`.
  - The first update seeds the EMA (Temperature.cpp:98), so a sustained `at(T)` reads T from ms 0.
  - The debounce check is `uint32_t(ms - since) >= kAlarmDebounceMs` (Temperature.cpp:83-84). Asserting at exactly 2500 catches the `>` mutation.
  - In AC-3 the Red timer only starts once t ≥ 130, because Amber is latched rather than Red (Temperature.cpp:70). That is why `t0` is defined from the reported temperature.
  - Prefer `config::kAlarmDebounceMs` to the literal 2500.
  - The build uses `-Werror`, so avoid sign-compare warnings.
- safety_notes: Tests only; covers invariant 4 (debounced escalation). No production change.
- size: S

### T2: Add wraparound and brief-fault latch characterisation tests (AC-4, AC-5)
- kind: test
- status: ready
- depends_on: [T1]
- parallel_safe: false
- needs_human_approval: false
- goal: Prove that debounce and fault-reset timing are unchanged across `uint32_t` millisecond wraparound, and that a fault shorter than `kFaultResetMs` keeps the latched Red alarm and the filter.
- files_may_touch:
  - tests/demo_temperature_test.cpp
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/test_calibration_tool.py
  - main/**
  - scripts/**
  - hardware/**
  - docs/**
  - build/
  - managed_components/
  - sdkconfig
- acceptance_criteria:
  - **AC-4 block.** Use a fresh state with `uint32_t` timestamps starting at `0xFFFFF000u`, advancing 250 ms per update.
    1. Sustained `at(139)`: `alarm == Normal` at start+2250 and `alarm == Red` at start+2500.
    2. Keep holding 139 °C. Start a sustained `kOpen` fault at a time chosen so that [fault_start, fault_start + `kFaultResetMs`] crosses `UINT32_MAX` → 0. Assert that it does: `fault_start > fault_start + kFaultResetMs` in `uint32_t`.
    3. At every fault update with elapsed time < `kFaultResetMs`: `alarm == Red`.
    4. At the first update with elapsed time ≥ `kFaultResetMs`: `status == OpenCircuit` and `alarm == Normal`.
  - **AC-5 block.**
    1. Use a fresh state and latch Red with `hold(s, at(139), ms, 4000)`.
    2. Feed `kOpen` for `kFaultResetMs − 250` ms, so updates fall at fault_start … fault_start + 2750. At each: `status == OpenCircuit` and `alarm == Red`.
    3. Send one `at(139)` sample at fault_start + 3000. Assert `alarm == Red`, `status == Red` and `has_temperature`.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites pass; Python OK
  - `git diff --stat` -> only `tests/demo_temperature_test.cpp`
- hardware_verification: none
- notes:
  - **Pitfall:** `hold()` (demo_temperature_test.cpp:22-26) computes `end = ms + duration_ms` in `unsigned`. Across the wrap, `end < ms`, so the loop does nothing and the test passes for the wrong reason. **Do not use `hold` across the wrap.** Use an explicit loop over a step count with `uint32_t ms += 250`.
  - Starting at `0xFFFFF000` leaves only 4096 ms before the wrap. AC-2's debounce therefore finishes before the wrap, and the fault-reset window is the part that must cross it. Example: start the fault at start+3000, so the window is start+3000 … start+6000 and wraps at start+4096.
  - The fault reset is at Temperature.cpp:92-93. The fault branch doesn't call `updateAlarm`, so the Red timer survives a brief fault (Temperature.cpp:90-96). That is why AC-5 gets Red immediately.
  - After `hold`, `ms` is one step past its last update.
  - Include `<cstdint>` and use a `u` suffix on the start constant (`-Werror`).
- safety_notes: Tests only; covers invariant 4 (brief faults keep the alarm) and REQ-T5. No production change.
- size: S

### T3: Add Cold, table-endpoint and max-tracking characterisation tests (AC-6, AC-7, AC-8)
- kind: test
- status: ready
- depends_on: [T2]
- parallel_safe: false
- needs_human_approval: false
- goal: Pin down the `Cold` status below `kWarmThresholdC`, the inclusive table endpoints, and the rule that `maximumC()` never decreases.
- files_may_touch:
  - tests/demo_temperature_test.cpp
- files_must_not_touch:
  - tests/temperature_test.cpp
  - tests/test_calibration_tool.py
  - main/**
  - scripts/**
  - hardware/**
  - docs/**
  - build/
  - managed_components/
  - sdkconfig
- acceptance_criteria:
  - **AC-6 block.** Use a fresh state and `hold(s, at(30), ms, 2000)`. Assert:
    - `status == Status::Cold`;
    - `has_temperature`;
    - `alarm == Status::Normal`;
    - `!gauge::isFault(status)`.
  - **AC-7 block.**
    - `temperatureFromResistance(1100.f, t)` returns true with `fabs(t − 20) < 0.01`.
    - `temperatureFromResistance(47.f, t)` returns true with `fabs(t − 140) < 0.01`.
    - `calibrationSide(1100.f) == 0` and `calibrationSide(47.f) == 0`.
  - **AC-8 block.**
    1. Use a fresh state. Hold `at(139)`, then hold `at(105)` for about 10000 ms so the filter settles.
    2. After every update, `maximumC()` is ≥ its previous value.
    3. At the end, `fabs(maximumC() − highest reported temperature) < 0.01`.
    4. The final reported temperature is below the maximum.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites pass; Python OK
  - `git diff --stat` -> only `tests/demo_temperature_test.cpp`
- hardware_verification: none
- notes:
  - The demo table is at Calibration.cpp:8-11 (20 °C/1100 Ω … 140 °C/47 Ω), and its endpoints are inclusive (Calibration.cpp:35, 62-63).
  - `maximumC()` starts at −999 (Temperature.h:43) and updates only on in-table readings (Temperature.cpp:114). Initialise "previous" from `s.maximumC()`.
- safety_notes: Tests only; covers invariant 2 at the table boundaries and REQ-T1/T4. No production change.
- size: S

### T4: Add statusText completeness and uniqueness test (AC-9)
- kind: test
- status: ready
- depends_on: [T3]
- parallel_safe: false
- needs_human_approval: false
- goal: Every `Status` has its own human-readable text, so a missing switch case (the "UNKNOWN" fallback) or a duplicate string fails the host tests.
- files_may_touch:
  - tests/temperature_test.cpp
- files_must_not_touch:
  - tests/demo_temperature_test.cpp
  - tests/test_calibration_tool.py
  - main/**
  - scripts/**
  - hardware/**
  - docs/**
  - build/
  - managed_components/
  - sdkconfig
- acceptance_criteria:
  - Loop i from 0 to `static_cast<int>(Status::Red)` and use `static_cast<Status>(i)`.
    - `statusText` returns non-null, non-empty text other than "UNKNOWN".
    - The strings are unique (pairwise `strcmp`).
  - Guard the count: assert `static_cast<int>(Status::Red) == 11`, with a comment telling whoever adds a `Status` to update this test.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites pass; Python OK
  - `git diff --stat` -> only `tests/temperature_test.cpp` for this task
- hardware_verification: none
- notes:
  - The enum is at Temperature.h:4-7 (`uint8_t`, contiguous, Red last), and `statusText` at Temperature.cpp:42-58.
  - Add `<cstring>`. This file builds without `GAUGE_DEMO`.
  - Match the loop index types to avoid `-Wsign-compare`.
- safety_notes: Tests only. A wrong or duplicated status text could hide a fault from the driver, and this test guards it. No production change.
- size: S

#### 6. Execution order
1. T1
2. T2
3. T3
4. T4

They run strictly in sequence. T1–T3 share a file, and every gate compiles both test files into fixed paths under `/tmp/caterham-oil-gauge-tests/`.

#### 7. Risks
- **R1 (AC-4 strength).** Only the fault-reset window crosses the wrap, so only it can catch a timing mutation there. Some signed mutations, such as `int32_t(ms)-int32_t(since)`, behave the same in practice and would survive. The reviewer should use a mutation that really differs, such as `ms >= fault_since_ + kFaultResetMs` or a 64-bit signed difference.
- **R2 (false pass).** `hold()` does nothing across a wrap. T2 forbids using it there, and the reviewer checks.
- **R3 (enum coverage).** AC-9 relies on Red being the last enumerator. The count guard helps, but a new enumerator still needs a manual test update.
- **R4 (float edges).** Readings round-trip through `forOhms`, so compare with margins. Exact equality is used only where AC-7 specifies ±0.01.
- **R5 (scope).** A defect found here is escalated, not fixed in `main/`.

#### 8. Out of scope / follow-ups
- Making `hold()` wrap-safe.
- Display and ADS1115 driver tests.
- Whether BelowRange should clear a latched alarm. This is issue #7, the next CR.
- A `Status::Count` sentinel, which would need a change to `main/`.

#### 9. Definition of done
- `./scripts/test_host.sh` exits 0.
- `/build` passes: host tests, plus the normal and demo firmware builds with no `main/` warnings. `main/` is unchanged; this is a sanity check.
- `git diff --stat main...HEAD` lists only `tests/demo_temperature_test.cpp` and `tests/temperature_test.cpp`, plus the `docs/tasks.md` bookkeeping.
- The reviewer's mutation check passes for AC-1 to AC-5. It is run on a **copy under `/tmp/caterham-oil-gauge-review/`**, never on the repo.
- No hardware verification is owed.
- The PR says `Closes #5`.

---

## Progress log

Newest last. One row per implementer run, review round, approval, escalation or hardware check.

| Date | Task | Event | Result | Notes |
|------|------|-------|--------|-------|
| 2026-10-27 | CR-1 | Planned (planner) | 4 test tasks, no open questions | Plan saved; DoD mutation wording corrected to /tmp copy |
| 2026-10-27 | T1 | Implemented (round 1) | done | AC-1..AC-3 blocks, +35 lines |
| 2026-10-27 | T1 | Review round 1 | pass + 1 minor | AC-3 skipped asserts before t0; sent back. Mutations: `>` debounce, shared Red timer, ±250 ms all caught |
| 2026-10-27 | T1 | Fix + review round 2 | pass | Pre-t0 Amber asserted; targeted Amber-drop mutation caught only by the new line |
