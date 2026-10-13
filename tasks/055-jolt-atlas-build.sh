#!/bin/bash
# Task 055 — one-off, unrelated to Quest: build and smoke-run ICME-Lab/jolt-atlas.
# The session sandbox cannot reach rustup.rs; the runner box can. Rust 1.95 is pinned by the
# repo's rust-toolchain.toml. Nothing under the queue checkout is touched except results/.
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
RES=$ROOT/results/055-jolt-atlas-build; mkdir -p "$RES"
T0=$(date +%s)
step(){ echo; echo "===== $* ($(( $(date +%s) - T0 ))s) ====="; }

step "host"
uname -a; nproc; free -g | head -2; df -h /tmp | tail -1

step "toolchain"
export RUSTUP_HOME=/tmp/jolt-rustup CARGO_HOME=/tmp/jolt-cargo
export PATH=$CARGO_HOME/bin:$HOME/.cargo/bin:$PATH
if command -v rustup >/dev/null 2>&1; then
  echo "rustup present: $(command -v rustup)"
  # If the system rustup lives in a read-only home, point at our own dirs anyway.
  RUSTUP_BIN=$(command -v rustup)
else
  echo "no rustup; installing to $RUSTUP_HOME / $CARGO_HOME"
  curl -sSf https://sh.rustup.rs -o /tmp/rustup-init.sh || { echo "cannot download rustup"; exit 1; }
  sh /tmp/rustup-init.sh -y --no-modify-path --default-toolchain none --profile minimal \
    || { echo "rustup install failed"; exit 1; }
  RUSTUP_BIN=$CARGO_HOME/bin/rustup
fi
"$RUSTUP_BIN" --version
"$RUSTUP_BIN" toolchain list

step "clone"
SRC=/tmp/jolt-atlas; rm -rf "$SRC"
git clone --depth 1 https://github.com/ICME-Lab/jolt-atlas.git "$SRC" || { echo "clone failed"; exit 1; }
cd "$SRC"
git log -1 --format='%H %cd %s' | tee "$RES/commit.txt"
cat rust-toolchain.toml

step "rustc version (rust-toolchain.toml should auto-install 1.95)"
rustc --version 2>&1 | tee "$RES/rustc.txt" || { echo "rustc unavailable"; exit 1; }
cargo --version

step "cargo build --release --workspace"
B0=$(date +%s)
cargo build --release --workspace 2>&1 | tee "$RES/build.log" | tail -40
BSTAT=${PIPESTATUS[0]}
echo "build exit=$BSTAT seconds=$(( $(date +%s) - B0 ))" | tee "$RES/build_status.txt"
[ "$BSTAT" = "0" ] || { echo "BUILD FAILED"; grep -n "^error" "$RES/build.log" | head -20; exit 1; }

step "examples available"
ls jolt-atlas-core/examples

step "run: microgpt example (smallest; no model download)"
E0=$(date +%s)
timeout 10m cargo run --release --package jolt-atlas-core --example microgpt 2>&1 \
  | tee "$RES/microgpt.log" | tail -60
ESTAT=${PIPESTATUS[0]}
echo "microgpt exit=$ESTAT seconds=$(( $(date +%s) - E0 ))" | tee "$RES/run_status.txt"

echo
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))"
[ "$ESTAT" = "0" ] || { echo "TASK 055 RED (build OK, microgpt exit $ESTAT)"; exit 1; }
echo "TASK 055 GREEN"
