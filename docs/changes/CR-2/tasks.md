# CR-2 task plan

Workflow and conventions: [docs/changes/README.md](../README.md). Task format: `.claude/agents/planner.md`.

## Current plan

### Plan for CR-2 (issue #7): Keep a latched alarm when a raw sample reads colder than the table

#### 1. Summary
**Goal.**
- At the moment, a raw sample colder than the calibration table clears a latched Amber or Red alarm on the first sample. The EMA filter then shows false in-table temperatures.
- With CR-2, such samples bypass the filter, the alarm and the maximum while an alarm is latched. The status is `BelowRange` with no number.
- After `kBelowRangeHoldMs` (10 000 ms) of such samples, the state resets as for a sustained fault (REQ-F2).

**Approach.**
- In `TemperatureState::update()` (`main/core/Temperature.cpp:88-121`), add a branch after the fault branch and `in_fault_=false`, and before the EMA update (line 98).
- The branch acts on the **raw** `r.status` from `convertSample()`.
- If `BelowRange && alarm_ != Normal`:
  - start or keep a hold timer;
  - while `uint32_t(ms - below_since_) < kBelowRangeHoldMs`, return early with `BelowRange`, no temperature and `alarm = alarm_`; the filter, the Amber and Red timers and the maximum are untouched;
  - once the hold time is reached, call `reset()` and fall through.
- Any sample that reaches the filter clears the hold.
- `convertSample` is unchanged.

**Alternative rejected.** Gating on the *filtered* `BelowRange` (line 103). The EMA has already passed through in-table values by then.

**Tasks:** 4 — T1 (impl: constant), T2 (test), T3 (impl: core), T4 (docs).

#### 2. Acceptance criteria → tasks
| AC | Tasks |
|----|-------|
| AC-1, AC-2, AC-3, AC-4, AC-6, AC-7 | T2 (test), T3 (impl) |
| AC-5 | T2 (preservation test), T3 (must keep it passing) |
| AC-8 | T1, T3 (verification), final `/build` |
| Mutation check | T2 (tests designed to catch each mutation), reviewer of T3 |
| Hardware decade-box check | T3 `hardware_verification` (human) |
| Docs (VALIDATION, CALIBRATION, spec §2 REQ-T3) | T4 |

#### 3. Assumptions
1. "Alarm latched" means `alarm_ != Status::Normal`. A pending debounce with alarm still `Normal` is not latched, so it behaves as it does today.
2. **Fault samples during a hold neither restart nor clear the hold timer.**
   - They take the existing fault early-return first, and a cold sample resets `in_fault_`.
   - So alternating fault and cold samples still reset after `kBelowRangeHoldMs`, and the alarm cannot be held for ever.
   - **Confirmed by the maintainer on 2026-10-27**, and pinned by the new AC-9 (in T2).
3. The sample that ends the hold falls through after `reset()`. It seeds the filter with the cold resistance, giving `BelowRange` with alarm `Normal`.
4. No display or CSV change is needed.
   - The display already renders `BelowRange` as `< min °C`, and the alarm colour takes precedence (`GaugeDisplay.cpp:79-96`).
   - The CSV logs status and alarm separately (`main/main.cpp:72`).
5. AC-3 timing: cold samples at `c0 … c0+hold−250`, then `at(139)` at `c0+hold`. That last sample is not cold, so it causes no reset.
6. T1's `Config.h` change is covered by the approval in `request.md`, so it is `needs_human_approval: false`.
7. The `docs/spec.md` §2 REQ-T3 edit is in T4, the last task before the PR. The caller changes the §3 status.

#### 4. Open questions
_Resolved by the maintainer on 2026-10-27 at plan review:_
1. Assumption 2 is confirmed. A test is added as **AC-9** in `request.md`, covered by T2.
2. `docs/ARCHITECTURE.md` is **added to T4's scope** (in request.md and T4).
3. Agent invariant 4 is updated in the three agent files to name the approved REQ-T3 below-range exception. This was done by the caller as a workflow change, outside CR-2.

#### 5. Tasks

### T1: Add kBelowRangeHoldMs to Config.h
- kind: impl
- status: done
- depends_on: []
- parallel_safe: false
- needs_human_approval: false  (covered by the approval in request.md)
- goal: Add the approved `constexpr uint32_t kBelowRangeHoldMs = 10000;` to `main/Config.h` with a short comment, so the T2 tests can refer to it by name. No behaviour change.
- files_may_touch:
  - main/Config.h
- files_must_not_touch:
  - tests/
  - main/core/
  - main/hal/
  - main/display/
  - main/main.cpp
  - hardware/
  - main/CalibrationData.h
  - build/
  - managed_components/
  - sdkconfig
  - sdkconfig.defaults
- acceptance_criteria:
  - The constant is in `namespace config`, typed `uint32_t`, with value exactly `10000`.
  - No other line of `Config.h` changes.
  - The constant is not used anywhere yet.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites print "passed"; Python OK
  - Firmware build -> succeeds; `grep -n 'main/.*warning:'` on its log prints nothing
  - Demo build -> succeeds; the same grep prints nothing
  - `git diff --stat <task base>` -> only `main/Config.h`, about 3 lines
- hardware_verification: none
- notes:
  - Place it next to `kFaultResetMs` (`main/Config.h:28-30`) and follow the same comment style.
  - Suggested comment: "While an alarm is latched, raw samples colder than the table are held for this long, then the state resets as for a sustained fault."
- safety_notes: Alarm-timing constant. The approved value is 10 000 ms; use no other value.
- size: S

### T2: Add failing tests for the below-range hold, plus preservation tests
- kind: test
- status: done
- depends_on: [T1]
- parallel_safe: false
- needs_human_approval: false
- goal: Add CR-2 AC-1 to AC-7 blocks to `tests/demo_temperature_test.cpp`.
  - They must **fail** on current code for AC-1, AC-2, AC-3 and AC-7, and **pass** for the preservation criteria AC-4, AC-5 and AC-6.
  - They must catch every mutation listed in request.md.
- files_may_touch:
  - tests/demo_temperature_test.cpp
- files_must_not_touch:
  - main/
  - scripts/
  - tests/temperature_test.cpp
  - tests/test_*.py
  - hardware/
  - docs/
  - build/
  - managed_components/
  - sdkconfig
  - main/CalibrationData.h
- acceptance_criteria:
  - **Existing code unchanged.** The existing lines 1-227 stay byte-for-byte the same (helpers and every CR-1 block). New code is only added:
    - new blocks between line 227 and the final `std::puts`;
    - at most one new file-scope `static const gauge::Sample kCold=forOhms(1500.f);`;
    - helper functions.
  - **Constant, not literal.** Tests use `config::kBelowRangeHoldMs`, never the literal 10000.
  - **Block order:** AC-5, AC-6, AC-4, then AC-1, AC-2, AC-3, AC-7, AC-9. Label the blocks "CR-2 AC-n".
  - **CR-2 AC-1.**
    - Latch Red with `hold(s,at(139),ms,4000)` and assert `alarm==Red`. Record `c0=ms`, `maximumC()` and `maximumAboveRange()`.
    - With explicit per-update assertions, for every `ms-c0 < kBelowRangeHoldMs`:
      - `alarm==Red`, `status==BelowRange`, `!has_temperature` and `!isFault(status)`;
      - the maximum and the above-range flag are unchanged.
  - **CR-2 AC-2.** The same as AC-1 with `at(120)` latched and `alarm==Amber` throughout.
  - **CR-2 AC-3.**
    - Latch Red at 139, then send `kCold` at `c0 … c0+hold−250`.
    - Then one `at(139)` at `c0+hold`. Assert `has_temperature`, `fabs(temperature_c−139)≤0.5`, `alarm==Red` and `status==Red`.
  - **CR-2 AC-4.**
    - Latch Red and send `kCold` from `c0`.
    - At the first update with `ms−c0 ≥ hold`: `alarm==Normal` and `status==BelowRange`.
    - Keep sending `kCold` for at least 5000 ms more, with the same assertions at every update.
  - **CR-2 AC-5.**
    - Latch Red at 139 for 4000 ms.
    - Ramp `at(139−0.25·i)` for i = 0..136, one update per 250 ms, then `at(105)` for 2000 ms.
    - Assert exactly 2 alarm changes, Red→Amber→Normal, ending `Normal`.
  - **CR-2 AC-6.**
    - Fresh state, `kCold` for at least `hold+2000` ms: `BelowRange`, `Normal` and no temperature at every update.
    - **Plus:** fresh state, `kCold` for 1000 ms, then one `at(70)`, which must still be `BelowRange` with no temperature. This shows the cold samples went into the filter, and catches the mutation "hold applied with no alarm latched".
  - **CR-2 AC-7.**
    - Fresh state with `uint32_t` timestamps from `0xFFFFE000u`. Latch Red with an explicit `uint32_t` loop of 17 updates, **not `hold()`**.
    - Assert that `c0+hold < c0` (the window crosses the wrap).
    - Repeat the AC-1 assertions while `uint32_t(ms−c0) < hold`.
    - Repeat the AC-4 assertions from the reset for at least 2000 ms more.
  - **CR-2 AC-9 (added at plan review).**
    - Latch Red at 139 °C and record `c0`. Then alternate `kCold` and `kOpen`, one per 250 ms update, starting with `kCold` at `c0`.
    - Assert `alarm==Red` at every update before `c0+kBelowRangeHoldMs`.
    - Assert `alarm==Normal` and `status==BelowRange` at the first `kCold` update at or after `c0+kBelowRangeHoldMs`.
    - `kOpen` already exists in the file; do not redefine it.
  - **Warnings.** The file stays warning-free under `-Wall -Wextra -Werror`.
- verification:
  - `./scripts/test_host.sh; echo "exit=$?"` -> the core suite passes. Then `demo_tests` **aborts on an assertion inside the CR-2 AC-1 block**, after the AC-5, AC-6 and AC-4 blocks have passed. The exit code is non-zero. This is the expected failing state before T3.
  - `git diff <task base> -- tests/demo_temperature_test.cpp | grep '^-' | grep -v '^---'` -> prints nothing (additions only)
  - `git diff --stat <task base>` -> only `tests/demo_temperature_test.cpp`
- hardware_verification: none
- notes:
  - Mutations to kill:
    - hold removed → AC-1 and AC-2 fail;
    - held samples fed into the EMA → AC-3 fails;
    - hold never ends → AC-4 fails;
    - additive or signed/widened comparison → AC-7 fails;
    - hold applied with no alarm latched → the second part of AC-6 fails;
    - a fault sample restarting the hold timer → AC-9 fails.
  - `hold()` (lines 23-27) is not wrap-safe. Copy the explicit-loop pattern of the CR-1 AC-4 block (lines 143-163).
  - `kCold=forOhms(1500)` is beyond the 1100 Ω end of the demo table.
- safety_notes: Tests only. They pin fail-closed alarm retention and REQ-C3 preservation.
- size: M

### T3: Hold the latched alarm on raw below-range samples in TemperatureState
- kind: impl
- status: done
- depends_on: [T2]
- parallel_safe: false
- needs_human_approval: false
- goal: Implement the amended REQ-T3 in `TemperatureState::update()`.
  - While an alarm is latched, a raw `BelowRange` sample bypasses the EMA, the alarm and the maximum.
  - After `kBelowRangeHoldMs` of such samples, the state resets as for a sustained fault.
- files_may_touch:
  - main/core/Temperature.cpp
  - main/core/Temperature.h
- files_must_not_touch:
  - tests/
  - scripts/
  - main/Config.h
  - main/core/Calibration.*
  - main/hal/
  - main/display/
  - main/main.cpp
  - hardware/
  - docs/
  - build/
  - managed_components/
  - sdkconfig
  - main/CalibrationData.h
- acceptance_criteria:
  - `./scripts/test_host.sh` passes, including every CR-1 and T2 block, with no test file modified.
  - `convertSample()`, `statusText()`, `isFault()`, `targetFor()` and `updateAlarm()` are unchanged.
  - The hold uses the raw `r.status` from `convertSample()`, not the filtered side, and requires `alarm_ != Normal`.
  - The elapsed-time test is exactly `uint32_t(ms - below_since_) >= config::kBelowRangeHoldMs`.
  - `reset()` clears the hold, and any sample that reaches the filter clears it too.
- verification:
  - `./scripts/test_host.sh` -> exits 0; both C++ suites print "passed"; Python OK
  - Firmware build and demo build -> both succeed; `grep -n 'main/.*warning:'` prints nothing for either
  - `git diff --quiet <task base> -- tests/ scripts/ main/Config.h; echo $?` -> `0`
  - `git diff --stat <task base>` -> only `main/core/Temperature.{cpp,h}`, about 25 lines
- hardware_verification: A human does this with a build that has a locally generated, **verified** calibration table, which must not be committed. Use a decade box:
  1. Set a resistance hotter than the red threshold until CRITICAL latches.
  2. Switch to about 1500 Ω (colder than the table).
  3. Check `< min °C` in red, with alarm `CRITICAL TEMP` in the CSV, for about 10 s, and never an in-table number.
  4. Check that it then clears to Normal while still showing `< min °C`.
  5. Repeat from an Amber latch.
  6. Log the result in the Progress log.
- notes:
  - `Temperature.h`: add `bool below_hold_=false; uint32_t below_since_=0;` beside `in_fault_`.
  - `reset()`: add `below_hold_=false;`.
  - After `in_fault_=false;` in `update()`:
    `if (r.status==Status::BelowRange && alarm_!=Status::Normal) { if(!below_hold_){below_hold_=true;below_since_=ms;} if (uint32_t(ms-below_since_)<config::kBelowRangeHoldMs) { r.has_temperature=false; r.temperature_c=0; r.alarm=alarm_; return r; } reset(); } else below_hold_=false;`
  - Don't touch `max_c_` or `max_above_range_` in the hold path.
  - The fault branch stays first (Assumption 2).
  - Update the comment in `update()`.
- safety_notes:
  - Strengthens fail-closed behaviour.
  - Keeps REQ-C3 (`BelowRange` is not added to `isFault()`; no number shown).
  - No extrapolation; `AboveRange` handling is unchanged.
  - De-escalation stays immediate except for the approved exception.
- size: S

### T4: Document the below-range hold and amend REQ-T3
- kind: docs
- status: ready
- depends_on: [T3]
- parallel_safe: false
- needs_human_approval: false
- goal: Bring the scoped docs in line with the new behaviour, and write the approved REQ-T3 wording into `docs/spec.md` §2.
- files_may_touch:
  - docs/spec.md
  - docs/VALIDATION.md
  - docs/CALIBRATION.md
  - docs/ARCHITECTURE.md
- files_must_not_touch:
  - main/
  - tests/
  - scripts/
  - hardware/
  - docs/changes/
  - README.md
  - build/
  - managed_components/
  - sdkconfig
  - main/CalibrationData.h
- acceptance_criteria:
  - `docs/spec.md` §2 REQ-T3 reads exactly the **New** wording from `docs/changes/CR-2/request.md`, and its evidence tag cites the CR-2 host tests. No other §2 requirement changes, and §3 is not edited.
  - `docs/VALIDATION.md` lists the below-range hold among the host-tested behaviours, and adds a hardware row: "Alarm latched, then sender colder than table (decade box)" → "`< min °C`, alarm colour kept for `kBelowRangeHoldMs` (10 s), never an in-table number, then clears" → "photo/log".
  - `docs/ARCHITECTURE.md` line 49 (below range is "not a fault and no alarm") keeps that meaning, and adds that a latched alarm is held for `kBelowRangeHoldMs`, then cleared. *Added at plan review.*
  - `docs/CALIBRATION.md` says a latched alarm is kept for `kBelowRangeHoldMs` (10 s) when the reading goes `BELOW CAL RANGE` (a probable wiring fault), then clears. `BelowRange` is still not a fault and shows no number.
- verification:
  - `grep -c 'kBelowRangeHoldMs' docs/spec.md docs/VALIDATION.md docs/CALIBRATION.md docs/ARCHITECTURE.md` -> each count ≥ 1
  - `grep -n 'REQ-T3' docs/spec.md` -> one §2 line containing "**Exception:**" and "10 000 ms"
  - `git diff --stat <task base>` -> only the four allowed doc files
  - `./scripts/test_host.sh` -> exits 0
- hardware_verification: none
- notes:
  - Copy the REQ-T3 wording verbatim; do not paraphrase an approved requirement.
- safety_notes: Documentation must not suggest that `BelowRange` is a fault, or that the hold applies when no alarm is latched.
- size: S

#### 6. Execution order
1. T1
2. T2
3. T3
4. T4

They run strictly in sequence: T1 and T3 both need firmware builds, and each task depends on the one before.

#### 7. Risks
- **"Hold with no alarm latched" mutation.** AC-6 as literally worded cannot detect it, so T2 adds the filter-observation assertion. The reviewer should check that it is there.
- **Agent invariant text.** Invariant 4 in the agent files says "de-escalation is immediate", so a reviewer could flag T3. The approved REQ-T3 amendment is the authority.
- **Fault and cold interleaving** (Assumption 2) is not pinned by any test.
- **Bench test needs a verified calibration table** that must not be committed. The reviewer checks `git diff` for `main/CalibrationData.h`.
- **AC-5 EMA lag.** The final settle at `at(105)` keeps the change count robust.
- **`hold()` is not wrap-safe.** AC-7 uses explicit loops.

#### 8. Out of scope / follow-ups
- A rate-of-change rule for sudden in-table drops.
- A "SENSOR CHECK" display text during the hold.

#### 9. Definition of done
- `./scripts/test_host.sh` exits 0.
- The normal and demo firmware builds have no `main/` warnings.
- The reviewer's mutation check confirms all six request.md mutations fail a test.
- `git diff main --stat` touches only `main/Config.h`, `main/core/Temperature.{h,cpp}`, `tests/demo_temperature_test.cpp`, `docs/spec.md`, `docs/VALIDATION.md`, `docs/CALIBRATION.md`, `docs/ARCHITECTURE.md` and the CR-2 folder. `main/CalibrationData.h` is unchanged.
- Hardware verification is still owed by a human, with the `hardware-test` label on #7 until it is done: the decade-box test from Red and from Amber, recorded in the Progress log.

---

## Findings

Review findings, kept by the caller. Columns and statuses: [README § Review rounds](../README.md#review-rounds).

| ID | Task | Raised | Severity | Title | Status | Closed | Notes |
|----|------|--------|----------|-------|--------|--------|-------|
| T2-F1 | T2 | 1 | major | No test that an in-table sample clears the hold; with `else below_hold_=false;` removed from the reference T3, a second cold episode drops a latched Red at once and the suite still passes | fixed | 2 | Re-arm block added (commit 26e72eb); the no-clear mutation is now caught, and the reference T3 passes |
| T3-F1 | T3 | 1 | nit | Hold comment says "sender wire open to a low value"; colder-than-table is high resistance (partly open or leaking connector) | fixed | 2 | Comment reworded (commit a0ff9a0); comment-only |
| T4-F1 | T4 | 1 | minor | ARCHITECTURE.md:49 / VALIDATION.md:5 say "10 s of such samples"; the hold runs from the first cold sample and fault samples do not restart it (AC-9), which no doc says | open | | Chosen for fix in T4 round 2 (bench-test accuracy) |
| T4-F2 | T4 | 1 | nit | ARCHITECTURE.md:49 "status stays `BelowRange`" should be "status is `BelowRange`" | open | | Chosen for fix in T4 round 2 |

---

## Progress log

Newest last. One row per implementer run, review round, approval, escalation or hardware check.

| Date | Task | Event | Result | Notes |
|------|------|-------|--------|-------|
| 2026-10-27 | CR-2 | Drafted (/spec-from-issue 7) | draft | AC-1..AC-3, AC-7 fail on current code (expected for a fix); AC-4..AC-6 pass |
| 2026-10-27 | CR-2 | Maintainer decisions | active | kBelowRangeHoldMs = 10 000 ms; REQ-T3 amendment approved (PurpleSentinel) |
| 2026-10-27 | CR-2 | Planned (planner, auto-started on activation) | 4 tasks: T1 impl, T2 test, T3 impl, T4 docs | Awaiting maintainer plan review: Assumption 2 and the ARCHITECTURE.md scope question |
| 2026-10-27 | CR-2 | Plan review (maintainer) | approved, "go" | Assumption 2 confirmed and pinned by new AC-9 (T2); docs/ARCHITECTURE.md added to T4; agent invariant 4 updated for the REQ-T3 exception |
| 2026-10-27 | T1 | Implemented (round 1, commit 05219f5) | done | +3 lines in Config.h. First handback was an empty "placeholder"; full report requested and received before review |
| 2026-10-27 | T1 | Review round 1 | pass | No findings. Gates re-run: host tests, both builds 0 main/ warnings. Mutation n/a (unused constant) |
| 2026-10-27 | T2 | Implemented (round 1, commit da03442) | done | +111 lines; fails in CR-2 AC-1 as intended after AC-5/6/4 pass |
| 2026-10-27 | T2 | Review round 1 | **fix** | Reference T3 passes all tests; all 6 request.md mutations caught; raised T2-F1 (major) |
| 2026-10-27 | T2 | Fix round 2 (commit 26e72eb) | T2-F1 fixed | +11 lines re-arm block; no extras |
| 2026-10-27 | T2 | Review round 2 | pass | T2-F1 fixed; all 11 mutations caught against the reference T3; suite still fails in CR-2 AC-1 as expected before T3 |
| 2026-10-27 | T3 | Implemented (round 1, commit e93aaed) | done | +14/−1 in Temperature.{cpp,h}; suite passes; both builds 0 main/ warnings |
| 2026-10-27 | T3 | Review round 1 | pass + 1 nit | All request.md mutations caught (M4c int32 and M9 are equivalent mutants); edge cases AboveRange-in-hold, NaN-in-hold and AboveRange-latched-then-cold behave fail-closed; raised T3-F1 (nit), chosen for fix |
| 2026-10-27 | CR-2 | Follow-up (not in scope) | logged | Debounce timers carry over across fault gaps and hold gaps, so one sample after a gap can confirm an escalation (fail-closed). Decide in a later CR whether to keep strict "held for kAlarmDebounceMs" semantics |
| 2026-10-27 | T3 | Fix round 2 (commit a0ff9a0) | T3-F1 fixed | Comment-only; no extras |
| 2026-10-27 | T3 | Review round 2 | pass | T3-F1 fixed; host tests and both builds (Temperature.cpp recompiled) pass, 0 main/ warnings. Hardware check still owed |
| 2026-10-27 | T4 | Implemented (round 1, commit e5c4e0e) | done | 4 docs; REQ-T3 pasted from request.md |
| 2026-10-27 | T4 | Review round 1 | pass + 1 minor + 1 nit | REQ-T3 checked byte-identical to the approved wording by script; docs match code; "partial" evidence tag correct (HW rows owed); raised T4-F1, T4-F2, both chosen for fix |
| 2026-10-27 | CR-2 | For maintainer (not actioned) | noted | Approved REQ-T3 wording ("If such samples continue for kBelowRangeHoldMs") is silent on interleaved faults; behaviour is pinned by AC-9. Tightening the wording needs maintainer approval |
