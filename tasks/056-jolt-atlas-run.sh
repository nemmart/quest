#!/bin/bash
# Task 056 — follow-up to 055: run microgpt with --trace-terminal (055 printed only the banner)
# and nanoGPT. Reuses the /tmp toolchain and build tree 055 left behind if still present.
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
RES=$ROOT/results/056-jolt-atlas-run; mkdir -p "$RES"
T0=$(date +%s)
step(){ echo; echo "===== $* ($(( $(date +%s) - T0 ))s) ====="; }

export RUSTUP_HOME=/tmp/jolt-rustup CARGO_HOME=/tmp/jolt-cargo
export PATH=$CARGO_HOME/bin:$PATH
SRC=/tmp/jolt-atlas
if [ ! -x "$CARGO_HOME/bin/cargo" ]; then
  step "toolchain gone; reinstalling"
  curl -sSf https://sh.rustup.rs -o /tmp/rustup-init.sh || { echo "cannot download rustup"; exit 1; }
  sh /tmp/rustup-init.sh -y --no-modify-path --default-toolchain none --profile minimal || exit 1
fi
if [ ! -d "$SRC/.git" ]; then
  step "source gone; recloning"
  git clone --depth 1 https://github.com/ICME-Lab/jolt-atlas.git "$SRC" || exit 1
fi
cd "$SRC"
git log -1 --format='%H %cd %s' | tee "$RES/commit.txt"
rustc --version

step "build (should be a no-op if 055's target/ survived)"
cargo build --release --package jolt-atlas-core --examples 2>&1 | tail -5 || { echo "BUILD FAILED"; exit 1; }

FAILS=0
run(){ local name=$1; shift
  step "run: $name $*"
  local E0=$(date +%s)
  timeout 15m cargo run --release --package jolt-atlas-core --example "$name" -- "$@" 2>&1 \
    | tee "$RES/$name.log" | tail -80
  local st=${PIPESTATUS[0]}
  echo "$name exit=$st seconds=$(( $(date +%s) - E0 ))" | tee -a "$RES/run_status.txt"
  [ "$st" = "0" ] || FAILS=$((FAILS+1))
}
run microgpt --trace-terminal
run nanoGPT --trace-terminal

echo; echo "wall_clock_seconds=$(( $(date +%s) - T0 ))"
cat "$RES/run_status.txt"
[ "$FAILS" = "0" ] || { echo "TASK 056 RED ($FAILS fails)"; exit 1; }
echo "TASK 056 GREEN"
