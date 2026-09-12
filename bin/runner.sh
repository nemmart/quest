#!/bin/bash
# runner.sh — the quest task runner. Polls the repo for unresulted tasks,
# runs them, pushes results. Changed ONCE by P49 (the overlap/rm-rf data-loss
# defect); otherwise this script does not change, tasks change. A change here
# takes effect only after the file is copied to the runner box and the unit is
# restarted -- until then the repo and the running poller disagree.
#
# Setup on the runner box (once), as a dedicated non-sudo user:
#   git clone https://x-access-token:<PAT>@github.com/nemmart/quest.git ~/queue
#   ~/queue/bin/runner.sh          # or install the systemd unit below
#
# Protocol:
#   tasks/NNN-name.sh        a self-contained bash script (run from repo root)
#   tasks/hold/              tasks here are IGNORED (manual-gate parking)
#   results/NNN-name/        created by this runner:
#       run.log              full stdout+stderr of the task
#       DONE or FAILED       marker file (FAILED includes the exit code)
#   Tasks run oldest-first, one at a time, under a timeout.
#
# systemd unit (optional, hardened) — save as
# /etc/systemd/system/quest-runner.service, then:
#   systemctl daemon-reload && systemctl enable --now quest-runner
#
#   [Unit]
#   Description=Quest task runner
#   After=network-online.target
#   [Service]
#   User=questrunner
#   ExecStart=/home/questrunner/queue/bin/runner.sh
#   Restart=always
#   RestartSec=30
#   NoNewPrivileges=yes
#   ProtectSystem=strict
#   ProtectHome=read-only
#   ReadWritePaths=/home/questrunner/queue /tmp
#   MemoryMax=16G
#   TasksMax=512
#   [Install]
#   WantedBy=multi-user.target

set -u
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
POLL_SECONDS="${POLL_SECONDS:-60}"
TASK_TIMEOUT="${TASK_TIMEOUT:-45m}"

cd "$REPO_DIR"

log() { echo "[runner $(date '+%H:%M:%S')] $*"; }

while true; do
  # Self-healing sync: the box is a disposable mirror of origin/main.
  # Any local results are pushed at the end of each task; between tasks we
  # hard-reset to origin so a divergence (e.g. a task rewritten upstream
  # while we produced a stale result) never wedges the loop.
  git fetch --quiet origin 2>/dev/null || { log "fetch failed; retrying"; sleep "$POLL_SECONDS"; continue; }
  if ! git merge-base --is-ancestor origin/main HEAD 2>/dev/null; then
    log "resyncing to origin/main"
    git reset --hard --quiet origin/main
    git clean -fdq results/ 2>/dev/null || true
  fi

  # oldest task (numeric order) that is not yet DONE. A task with only a
  # FAILED result (e.g. a bug in the task script, since fixed and pushed)
  # is retried — up to MAX_ATTEMPTS times — instead of being blocked
  # forever by the stale result dir. A DONE marker means never re-run.
  MAX_ATTEMPTS="${MAX_ATTEMPTS:-3}"
  task=""
  for t in $(ls tasks/*.sh 2>/dev/null | sort); do
    name=$(basename "$t" .sh)
    [ -f "results/$name/DONE" ] && continue          # succeeded: skip
    if [ -d "results/$name" ]; then                  # a prior FAILED attempt
      attempts=$(cat "results/$name/ATTEMPTS" 2>/dev/null || echo 0)
      [ "$attempts" -ge "$MAX_ATTEMPTS" ] && continue # give up after N tries
    fi
    task="$t"; break
  done

  if [ -z "$task" ]; then
    sleep "$POLL_SECONDS"
    continue
  fi

  name=$(basename "$task" .sh)
  # attempt bookkeeping (survives across runs via the committed result dir)
  attempts=$(cat "results/$name/ATTEMPTS" 2>/dev/null || echo 0)
  attempts=$((attempts + 1))
  log "running $name (attempt $attempts/$MAX_ATTEMPTS)"
  # P49: guard BEFORE the destructive step. A battery task holds this lock for
  # its whole run; with `Restart=always` in the unit, a restarted runner would
  # otherwise pick the same task up again, rm -rf the in-flight attempt's
  # results, and then be refused by the task's own overlap guard -- leaving
  # only the refusal message and burning an attempt. That destroyed task 053's
  # results twice (see results/053-p49-stage-b/RESTORED.md). Take the lock to
  # find out whether a task is live, then RELEASE it so the task can take it.
  exec 8>/tmp/quest-parallel-battery.lock
  if ! flock -n 8; then
    exec 8>&-
    log "a task is still holding the battery lock; deferring $name (no results touched)"
    sleep "$POLL_SECONDS"
    continue
  fi
  flock -u 8; exec 8>&-

  # Deletion is recoverable rather than final: a mistaken wipe of a committed
  # result has cost this project three incidents in one day.
  if [ -d "results/$name" ]; then
    mv "results/$name" "/tmp/quest-results-backup-$name-$(date +%s)" 2>/dev/null \
      || rm -rf "results/$name"
  fi
  mkdir -p "results/$name"
  echo "$attempts" > "results/$name/ATTEMPTS"

  # Run from repo root, everything captured. Timeout guards runaways.
  if timeout "$TASK_TIMEOUT" bash "$task" >"results/$name/run.log" 2>&1; then
    touch "results/$name/DONE"
    log "$name DONE"
  else
    rc=$?
    echo "exit=$rc" > "results/$name/FAILED"
    log "$name FAILED (exit $rc, attempt $attempts/$MAX_ATTEMPTS)"
  fi

  # Sep 5 2026: unstage anything a task may have left in the index (a
  # `git checkout <tree> -- paths` stages them) so results commits carry
  # ONLY results.  Tasks should use bin/task_source.sh instead.
  git reset --quiet
  git add -A "results/$name"
  git -c user.name="quest-runner" -c user.email="runner@localhost" commit --quiet -m "results: $name" || true
  # push with retry; a lost race just re-pulls
  for i in 1 2 3; do
    git push --quiet 2>/dev/null && break
    git pull --quiet --rebase 2>/dev/null
    sleep 5
  done
done
