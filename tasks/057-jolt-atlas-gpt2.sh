#!/bin/bash
# Task 057 — jolt-atlas GPT-2: download the HF ONNX export (README "Getting Started"),
# trace-only example, then prove+verify. Reuses 055/056's /tmp toolchain and tree.
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
RES=$ROOT/results/057-jolt-atlas-gpt2; mkdir -p "$RES"
T0=$(date +%s)
step(){ echo; echo "===== $* ($(( $(date +%s) - T0 ))s) ====="; }

export RUSTUP_HOME=/tmp/jolt-rustup CARGO_HOME=/tmp/jolt-cargo
export PATH=$CARGO_HOME/bin:$PATH
export PIP_CACHE_DIR=/tmp/jolt-pip-cache HF_HOME=/tmp/jolt-hf XDG_CACHE_HOME=/tmp/jolt-xdg
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
rustc --version; python3 --version

step "python venv"
VENV=/tmp/jolt-venv
if [ ! -x "$VENV/bin/python" ]; then
  python3 -m venv "$VENV" || { echo "venv creation failed (python3-venv missing?)"; exit 1; }
fi
"$VENV/bin/python" -m pip install --quiet --upgrade pip 2>&1 | tail -2

step "download_gpt2.py (optimum export; big pip install on first run)"
D0=$(date +%s)
"$VENV/bin/python" scripts/download_gpt2.py 2>&1 | tee "$RES/download.log" | tail -30
DSTAT=${PIPESTATUS[0]}
echo "download exit=$DSTAT seconds=$(( $(date +%s) - D0 ))" | tee "$RES/download_status.txt"
ls -la atlas-onnx-tracer/models/gpt2/ 2>&1 | tee -a "$RES/download_status.txt"
[ "$DSTAT" = "0" ] && [ -f atlas-onnx-tracer/models/gpt2/network.onnx ] || { echo "DOWNLOAD FAILED"; grep -iE "error|Traceback" "$RES/download.log" | tail -20; exit 1; }

FAILS=0
run(){ local pkg=$1 name=$2 tag=$3; shift 3
  step "run: $pkg/$name $*"
  local E0=$(date +%s)
  timeout 20m cargo run --release --package "$pkg" --example "$name" -- "$@" 2>&1 \
    | tee "$RES/$tag.log" | sed 's/\x1b\[[0-9;]*m//g' | grep -vE "^\s*Compiling" | tail -40
  local st=${PIPESTATUS[0]}
  echo "$tag exit=$st seconds=$(( $(date +%s) - E0 ))" | tee -a "$RES/run_status.txt"
  [ "$st" = "0" ] || FAILS=$((FAILS+1))
}
run atlas-onnx-tracer gpt2 trace
run jolt-atlas-core   gpt2 prove --trace-terminal

step "summary"
sed 's/\x1b\[[0-9;]*m//g' "$RES/prove.log" | grep -E "INFO (ONNXProof::prove|ONNXProof::verify|AtlasProverPreprocessing::gen): jolt_atlas_core::onnx_proof[a-z:_]*: close|setup_prover: .*close|Proof verified|max_num_vars" | sed 's/.*INFO //' | tee "$RES/summary.txt"
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))"
cat "$RES/run_status.txt"
[ "$FAILS" = "0" ] || { echo "TASK 057 RED ($FAILS fails)"; exit 1; }
echo "TASK 057 GREEN"
