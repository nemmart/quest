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
  git pull --quiet --ff-only 2>/dev/null || { log "pull failed; retrying"; sleep "$POLL_SECONDS"; continue; }

  # oldest task (numeric order) with no result dir
  task=""
  for t in $(ls tasks/*.sh 2>/dev/null | sort); do
    name=$(basename "$t" .sh)
    [ -d "results/$name" ] || { task="$t"; break; }
  done

  if [ -z "$task" ]; then
    sleep "$POLL_SECONDS"
    continue
  fi

  name=$(basename "$task" .sh)
  log "running $name"
  mkdir -p "results/$name"

  # Run from repo root, everything captured. Timeout guards runaways.
  if timeout "$TASK_TIMEOUT" bash "$task" >"results/$name/run.log" 2>&1; then
    touch "results/$name/DONE"
    log "$name DONE"
  else
    rc=$?
    echo "exit=$rc" > "results/$name/FAILED"
    log "$name FAILED (exit $rc)"
  fi

  git add -A "results/$name"
  git commit --quiet -m "results: $name" || true
  # push with retry; a lost race just re-pulls
  for i in 1 2 3; do
    git push --quiet 2>/dev/null && break
    git pull --quiet --rebase 2>/dev/null
    sleep 5
  done
done
