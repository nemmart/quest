#!/bin/bash
# Task 063 (062 retried: transformers 4.51.3 — GPT-2 got the Trilu/Range/Equal causal-mask builder in 4.52, for eager and SDPA alike; 4.51 still uses the constant bias buffer + where)*min form) — jolt-atlas GPT-2: download the HF ONNX export (README "Getting Started"),
# trace-only example, then prove+verify. Reuses 055/056's /tmp toolchain and tree.
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
RES=$ROOT/results/063-jolt-atlas-gpt2; mkdir -p "$RES"
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
# 057: the box's python3 has no ensurepip (python3-venv not installed), so the venv came up
# without pip. Build it --without-pip and bootstrap pip from get-pip.py instead.
VENV=/tmp/jolt-venv
if ! "$VENV/bin/python" -m pip --version >/dev/null 2>&1; then
  rm -rf "$VENV"
  python3 -m venv --without-pip "$VENV" || { echo "venv creation failed"; exit 1; }
  curl -sSf https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py || { echo "cannot download get-pip.py"; exit 1; }
  "$VENV/bin/python" /tmp/get-pip.py --quiet || { echo "get-pip failed"; exit 1; }
fi
"$VENV/bin/python" -m pip --version

step "pip: cpu torch + optimum 1.27 (058: the script's optimum[exporters] no longer resolves; 2.x dropped the extra)"
P0=$(date +%s)
"$VENV/bin/python" -m pip install --quiet "torch==2.8.0" --index-url https://download.pytorch.org/whl/cpu 2>&1 | tail -5 || { echo "torch install failed"; exit 1; }
"$VENV/bin/python" -m pip install --quiet "optimum[exporters,onnxruntime]==1.27.0" "transformers==4.51.3" 2>&1 | tail -5 || { echo "optimum install failed"; exit 1; }
"$VENV/bin/python" -m pip list 2>/dev/null | grep -iE "^(torch|optimum|transformers|onnx|onnxruntime) " | tee "$RES/pip_versions.txt"
echo "pip seconds=$(( $(date +%s) - P0 ))"

step "export gpt2 -> onnx (the script's export_model + rename_network, by hand)"
D0=$(date +%s)
MD=atlas-onnx-tracer/models/gpt2
rm -rf $MD   # 062 export used transformers 4.52; redo it
if [ ! -f $MD/network.onnx ]; then
  "$VENV/bin/python" - $MD <<'PYEOF' 2>&1 | tee "$RES/download.log" | tail -30
import sys
from transformers import AutoModelForCausalLM, AutoTokenizer
from optimum.exporters.onnx import onnx_export_from_model
out = sys.argv[1]
model = AutoModelForCausalLM.from_pretrained("gpt2", attn_implementation="eager")
print("attn impl:", model.config._attn_implementation)
AutoTokenizer.from_pretrained("gpt2").save_pretrained(out)
onnx_export_from_model(model, output=out, task="text-generation-with-past", do_validation=False)
PYEOF
  DSTAT=${PIPESTATUS[0]}
  [ "$DSTAT" = "0" ] && [ -f $MD/model.onnx ] && mv $MD/model.onnx $MD/network.onnx
fi
echo "export seconds=$(( $(date +%s) - D0 ))" | tee "$RES/download_status.txt"
ls -la $MD/ 2>&1 | tee -a "$RES/download_status.txt"
"$VENV/bin/python" - $MD/network.onnx <<'PYEOF' | tee "$RES/op_census.txt"
import onnx, sys, collections
m = onnx.load(sys.argv[1], load_external_data=False)
c = collections.Counter(n.op_type for n in m.graph.node)
print("onnx opset:", [(o.domain, o.version) for o in m.opset_import])
print("op census:", " ".join(f"{k}:{v}" for k, v in sorted(c.items())))
PYEOF
[ -f $MD/network.onnx ] || { echo "EXPORT FAILED"; grep -iE "error|Traceback" "$RES/download.log" | tail -20; exit 1; }

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
run jolt-atlas-core   gpt2 prove --trace-terminal

step "summary"
sed 's/\x1b\[[0-9;]*m//g' "$RES/prove.log" | grep -E "INFO (ONNXProof::prove|ONNXProof::verify|AtlasProverPreprocessing::gen): jolt_atlas_core::onnx_proof[a-z:_]*: close|setup_prover: .*close|Proof verified|max_num_vars" | sed 's/.*INFO //' | tee "$RES/summary.txt"
echo "wall_clock_seconds=$(( $(date +%s) - T0 ))"
cat "$RES/run_status.txt"
[ "$FAILS" = "0" ] || { echo "TASK 063 RED ($FAILS fails)"; exit 1; }
echo "TASK 063 GREEN"
