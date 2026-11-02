#!/bin/bash
# Task 095 -- meshsim WORKER18 (W5a): build the ACT4 architectural tests (RISC-V International riscv-arch-test,
# RV64IMAC unprivileged) exactly as upstream Jolt 17a9459 does (tests/arch-tests/, Makefile arch-tests-generate,
# scripts/bootstrap's tool versions), and run them through upstream's jolt-emu for its own verdicts.
#   1. tools: xpack riscv-none-elf-gcc v15.2.0-1, sail-riscv 2026-04-20-40d25df, mise v2026.10.25 (Ruby 3.4.9, uv 0.11.6
#      per ACT4's .mise.toml); the udb gem bundle
#   2. riscv-arch-test at a7c9930 (the submodule commit at jolt 17a9459), generated with upstream's
#      tests/arch-tests/jolt/test_config.yaml (embedded below, SHA-256 checked; include_priv_tests: false)
#   3. the ELFs (excluding *.sig.elf, as run.sh does) packed into results/: act4-elfs.tar.xz, elfs.txt (SHA-256 each)
#   4. jolt-emu (cargo build -p tracer --bin jolt-emu, debug, as upstream) on every ELF, skipped ones included:
#      jolt-emu-results.txt (name, exit code; 0 = pass), timeout 60 s each as run.sh
# Expected 15-30 min. Downloads from GitHub releases and rubygems.org; nothing is installed outside /tmp/w18-095.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/095-w18-act4-build; mkdir -p "$RES"
W=/tmp/w18-095; rm -rf "$W"; mkdir -p "$W/tools"
WANT=043b16ea647017f0d6e8af2c19db88a60284c1002b399b605ae96d6d22c6e353
sed -n '/^__KIT_BEGIN__$/,/^__KIT_END__$/p' "$0" | sed '1d;$d' | base64 -d > "$W/kit.tgz" || exit 2
GOT=$(sha256sum "$W/kit.tgz" | cut -d' ' -f1); echo "kit sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "kit sha256 mismatch (want $WANT)"; exit 2; }
tar -xzf "$W/kit.tgz" -C "$W" || exit 2
echo "start $(date -u) on $(hostname), $(nproc) threads"
T=$W/tools
curl -fsSL --retry 5 https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v15.2.0-1/xpack-riscv-none-elf-gcc-15.2.0-1-linux-x64.tar.gz | tar -xz -C $T || { echo "gcc download failed"; exit 1; }
mkdir -p $T/sail && curl -fsSL --retry 5 https://github.com/riscv/sail-riscv/releases/download/2026-04-20-40d25df/sail-riscv-Linux-x86_64.tar.gz | tar -xz -C $T/sail --strip-components=1 || { echo "sail download failed"; exit 1; }
mkdir -p $T/bin && curl -fsSL --retry 5 -o $T/bin/mise https://github.com/jdx/mise/releases/download/v2026.10.25/mise-v2026.10.25-linux-x64 && chmod +x $T/bin/mise || { echo "mise download failed"; exit 1; }
export PATH=$T/bin:$T/xpack-riscv-none-elf-gcc-15.2.0-1/bin:$T/sail/bin:$PATH
export MISE_YES=1 MISE_DATA_DIR=$W/mise-data MISE_CACHE_DIR=$W/mise-cache MISE_CONFIG_DIR=$W/mise-config MISE_STATE_DIR=$W/mise-state
riscv-none-elf-gcc --version | head -1; sail_riscv_sim --version; mise --version
git clone -q --filter=blob:none https://github.com/riscv/riscv-arch-test.git $W/act4 && (cd $W/act4 && git checkout -q a7c99303516f4e668f7488f172043392e23b9dfd) || { echo "act4 clone failed"; exit 1; }
cd $W/act4 && mise trust .mise.toml && mise install || { echo "mise install failed"; exit 1; }
ok=0; for a in 1 2 3 4 5; do (cd framework/src/act/data && mise exec -- bundle install) && { ok=1; break; }; sleep $((a * 15)); done
[ $ok = 1 ] || { echo "bundle install failed"; exit 1; }
echo "generate start $(date -u)"
make -j$(nproc) CONFIG_FILES=$W/tests/arch-tests/jolt/test_config.yaml WORKDIR=$W/work > $RES/generate.log 2>&1; GEN=$?
echo "generate exit $GEN at $(date -u)"; tail -20 $RES/generate.log
cd $W/work
find . -type f -name '*.elf' ! -name '*.sig.elf' | sort > $W/elfs.list
echo "$(wc -l < $W/elfs.list) ELFs"
xargs sha256sum < $W/elfs.list > $RES/elfs.txt
tar -cJf $RES/act4-elfs.tar.xz -T $W/elfs.list; ls -la $RES/act4-elfs.tar.xz
[ $GEN = 0 ] || exit 1
# upstream's own verdicts: jolt-emu at 17a9459, debug, as the Makefile builds it
if ! command -v cargo >/dev/null 2>&1; then
  export RUSTUP_HOME=$W/rustup CARGO_HOME=$W/cargo
  curl -sSf https://sh.rustup.rs -o $W/rustup-init.sh && sh $W/rustup-init.sh -y --no-modify-path --profile minimal >/dev/null || { echo "rustup failed"; exit 1; }
  export PATH=$CARGO_HOME/bin:$PATH
fi
git clone -q --filter=blob:none https://github.com/a16z/jolt.git $W/jolt && (cd $W/jolt && git checkout -q 17a9459082ec62fe8b50d61cc1f38cc060bd20f8) || { echo "jolt clone failed"; exit 1; }
(cd $W/jolt && cargo build -q -p tracer --bin jolt-emu) || { echo "jolt-emu build failed"; exit 1; }
EMU=$W/jolt/target/debug/jolt-emu
while read -r e; do n=$(basename "$e" .elf); timeout 60 $EMU "$W/work/$e" > /dev/null 2>&1; echo "$n $?"; done < $W/elfs.list > $RES/jolt-emu-results.txt
echo "jolt-emu: $(awk '$2==0' $RES/jolt-emu-results.txt | wc -l) pass, $(awk '$2!=0' $RES/jolt-emu-results.txt | wc -l) not, of $(wc -l < $RES/jolt-emu-results.txt)"
awk '$2!=0' $RES/jolt-emu-results.txt
echo "done $(date -u)"
rm -rf "$W"
exit 0
__KIT_BEGIN__
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
g8FgMN6Lfw9p2fMA8AAA
__KIT_END__
