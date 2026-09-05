#!/bin/bash
# runner.sh — the quest task runner. Polls the repo for unresulted tasks,
# runs them, pushes results. This script never changes; tasks change.
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
  rm -rf "results/$name"            # clear any stale FAILED/run.log from a prior try
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
