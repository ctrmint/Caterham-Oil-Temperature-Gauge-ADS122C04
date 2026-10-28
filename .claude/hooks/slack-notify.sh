#!/usr/bin/env bash
# slack-notify.sh - status-only Slack notifications for the change-request workflow.
#
# Who calls it: the main session that runs the change-request lifecycle
# (docs/changes/README.md, "Slack notifications"). The planner, implementer and reviewer
# agents never call it. Every lifecycle type takes cr=CR-<n>, because task numbers restart
# at T1 in each change request.
#
#   slack-notify.sh test
#   slack-notify.sh plan-ready      cr=CR-3 tasks=4            needs you: review the plan
#   slack-notify.sh approval-needed cr=CR-3 task=T1            needs you: needs_human_approval task
#   slack-notify.sh task-start      cr=CR-3 task=T2 round=1
#   slack-notify.sh gates           cr=CR-3 task=T2 host=pass build=pass demo=skip   (pass|fail|skip)
#   slack-notify.sh review          cr=CR-3 task=T2 round=1 verdict=fix blockers=0 majors=1
#   slack-notify.sh escalation      cr=CR-3 task=T2 reason=criterion-gap               needs you
#   slack-notify.sh task-done       cr=CR-3 task=T2 rounds=2
#   slack-notify.sh pr-ready        cr=CR-3 pr=#14 ci=pass     needs you: merge (or ci=fail)
#   slack-notify.sh bench-owed      cr=CR-3 issue=#15 step=below-range-hold
#   slack-notify.sh cr-done         cr=CR-3 tasks=4 findings=5 pr=#14
#
# As a Claude Code hook (reads the hook JSON on stdin; registered in .claude/settings.json):
#   slack-notify.sh needs-attention    Notification hook: idle, permission or question prompts
#   slack-notify.sh stopped            Stop hook. OFF by default (NOTIFY_STOP=1 enables it):
#                                      Stop fires at the end of every turn, including turns that
#                                      end while background agents are still working, so it
#                                      sends false "waiting for you" messages.
#
# Setup:
#   1. Put the Slack incoming-webhook URL in ~/.config/oil-gauge/slack_webhook
#      (override with SLACK_WEBHOOK_FILE), then chmod 600 that file.
#   2. Needs bash, curl and jq.
#   3. Check the wording without posting:  NOTIFY_DRY_RUN=1 .claude/hooks/slack-notify.sh test
#   4. Post one real test message:          .claude/hooks/slack-notify.sh test
#
# Design rules:
#   - STATUS ONLY. Every value passes through clean(): no newlines, quotes, <, >, &, @, !,
#     backticks, * or _, capped at 120 characters. Never pass agent output, diffs, finding
#     text or error text. Pass short codes.
#   - NEVER FAILS THE WORKFLOW. Always exits 0, with a 5 second network timeout.
#   - The webhook URL lives outside the repo, is checked to be a hooks.slack.com URL, is
#     passed to curl through a config stream (never on the command line, where `ps` shows it)
#     and is never logged.
#   - The webhook file cannot be fully hidden from agents: they run as your user and have
#     Bash. The deny rules in .claude/settings.json are a speed bump, not a guarantee. An
#     incoming webhook can only post to its one channel, so if the URL ever shows up in a
#     transcript or log, rotate it in Slack.
#   - Failures are logged (type and result only) to ~/.local/state/oil-gauge/notify.log.

set -u

WEBHOOK_FILE="${SLACK_WEBHOOK_FILE:-$HOME/.config/oil-gauge/slack_webhook}"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/oil-gauge"
LABEL="${NOTIFY_LABEL:-oil-gauge}"
THROTTLE_SECS="${NOTIFY_THROTTLE_SECS:-60}"   # applies to hook types only

mkdir -p "$STATE_DIR" 2>/dev/null && chmod 700 "$STATE_DIR" 2>/dev/null

log() { printf '%s %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$*" >>"$STATE_DIR/notify.log" 2>/dev/null; }

# Keep a small safe character set only, then cap the length. No * or _ (Slack bold/italics).
clean() { printf '%s' "$1" | tr -cd 'A-Za-z0-9 .:/#,+()=-' | cut -c1-120; }

TYPE="${1:-}"
[ $# -gt 0 ] && shift
ARGS=("$@")

# get KEY -> cleaned value of KEY=... from the arguments, or "?" if absent.
get() {
  local key="$1" a v
  for a in ${ARGS[@]+"${ARGS[@]}"}; do
    case "$a" in
      "$key="*) v="$(clean "${a#*=}")"; printf '%s' "${v:-?}"; return ;;
    esac
  done
  printf '?'
}

mark() {
  case "$1" in
    pass) printf '✅' ;;
    fail) printf '❌' ;;
    skip|skipped|na) printf '➖' ;;
    *) printf '❔' ;;
  esac
}

# "CR-3 T2", the subject of every lifecycle message.
subject() { printf '%s %s' "$(get cr)" "$(get task)"; }

# Hook types read their JSON from stdin (2 second limit, so a manual run cannot hang).
HOOK_JSON=""
case "$TYPE" in
  needs-attention|stopped) IFS= read -r -t 2 -d '' HOOK_JSON || true ;;
esac

hook_field() {
  [ -n "$HOOK_JSON" ] || return 0
  command -v jq >/dev/null 2>&1 || return 0
  printf '%s' "$HOOK_JSON" | jq -r "$1 // empty" 2>/dev/null
}

project_name() { clean "$(basename "${CLAUDE_PROJECT_DIR:-$PWD}")"; }

MSG=""
case "$TYPE" in
  test)
    MSG="✅ Slack notifications are working" ;;

  plan-ready)
    MSG="🙋 $(get cr) plan ready for your review: $(get tasks) tasks. Say go to start" ;;

  approval-needed)
    MSG="🙋 $(subject) needs your approval before it can start" ;;

  task-start)
    MSG="⚙️ $(subject) started (round $(get round))" ;;

  gates)
    h="$(get host)"; b="$(get build)"; d="$(get demo)"
    ok="✅"
    for s in "$h" "$b" "$d"; do
      case "$s" in pass|skip|skipped|na) ;; *) ok="❌" ;; esac
    done
    MSG="$ok $(subject) gates: host tests $(mark "$h"), build $(mark "$b"), demo build $(mark "$d")" ;;

  review)
    v="$(get verdict)"
    case "$v" in pass) ic="✅" ;; fix) ic="🔁" ;; escalate) ic="🛑" ;; *) ic="❔" ;; esac
    MSG="$ic $(subject) review round $(get round): $v (blockers $(get blockers), majors $(get majors))" ;;

  escalation)
    MSG="🛑 $(subject) escalated, needs your decision: $(get reason)" ;;

  task-done)
    MSG="✅ $(subject) done, passed review in $(get rounds) rounds" ;;

  pr-ready)
    ci="$(get ci)"
    case "$ci" in
      pass) MSG="🙋 $(get cr) PR $(get pr) is ready: CI ✅, waiting for your merge" ;;
      fail) MSG="❌ $(get cr) PR $(get pr): CI failed, being investigated" ;;
      *)    MSG="⏳ $(get cr) PR $(get pr) opened, CI $ci" ;;
    esac ;;

  bench-owed)
    MSG="🔧 $(get cr) bench check owed: $(get step) (issue $(get issue))" ;;

  cr-done)
    MSG="🏁 $(get cr) merged: $(get tasks) tasks, $(get findings) findings fixed, PR $(get pr)" ;;

  needs-attention)
    case "$(hook_field .notification_type)" in
      auth_success) exit 0 ;;
      permission_prompt) what="is waiting for your approval" ;;
      idle_prompt) what="is idle and waiting for you" ;;
      elicitation_dialog) what="has a question for you" ;;
      agent_needs_input) what="has an agent waiting for input" ;;
      *) what="needs your attention" ;;
    esac
    MSG="🙋 Claude Code $what ($(project_name))" ;;

  stopped)
    [ "${NOTIFY_STOP:-0}" = "1" ] || exit 0
    # Avoid loops: if this Stop was itself caused by a stop hook, say nothing.
    [ "$(hook_field .stop_hook_active)" = "true" ] && exit 0
    MSG="⏸️ Claude Code stopped ($(project_name))" ;;

  *)
    log "unknown type: $(clean "$TYPE")"
    echo "slack-notify: unknown type '$(clean "$TYPE")'" >&2
    exit 0 ;;
esac

MSG="*$(clean "$LABEL")* $MSG"

# Dry run: print and stop. Nothing is read or sent.
if [ "${NOTIFY_DRY_RUN:-0}" = "1" ]; then
  printf '%s\n' "$MSG"
  exit 0
fi

# Throttle repeated hook messages so a chatty session cannot flood the channel.
case "$TYPE" in
  needs-attention|stopped)
    stamp="$STATE_DIR/last-$TYPE"
    now="$(date +%s)"
    last="$(cat "$stamp" 2>/dev/null || echo 0)"
    case "$last" in ''|*[!0-9]*) last=0 ;; esac
    if [ $((now - last)) -lt "$THROTTLE_SECS" ]; then exit 0; fi
    printf '%s' "$now" >"$stamp" 2>/dev/null ;;
esac

for tool in curl jq; do
  command -v "$tool" >/dev/null 2>&1 || { log "missing dependency: $tool"; exit 0; }
done

[ -r "$WEBHOOK_FILE" ] || { log "webhook file not readable"; exit 0; }

perms="$(ls -ld "$WEBHOOK_FILE" 2>/dev/null | cut -c5-10)"
[ "$perms" = "------" ] || echo "slack-notify: warning: webhook file should be chmod 600" >&2

URL="$(tr -d '[:space:]' <"$WEBHOOK_FILE")"
case "$URL" in
  https://hooks.slack.com/services/*) ;;
  *) log "webhook file does not contain a hooks.slack.com URL"; exit 0 ;;
esac

# The URL goes to curl as a config stream (-K), so it never appears in the process list.
resp="$(jq -n --arg text "$MSG" '{text:$text}' |
  curl -sS --max-time 5 -X POST -H 'Content-type: application/json' --data @- \
    -K <(printf 'url = "%s"\n' "$URL") 2>/dev/null)" \
  || resp="curl-failed"

[ "$resp" = "ok" ] || log "post failed type=$(clean "$TYPE") response=$(clean "$resp")"

exit 0
