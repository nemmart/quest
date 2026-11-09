#!/bin/bash
# Task 165-w16-boot-end -- meshsim sim/ WORKER16 (branch sim-w16). Not timing; a minute or two.
# The boot-end point of M3's references (sim/tests/golden-m3, AUDIT-M3 section 7): every unit's line at cycle 7,222
# (`coresBooted`) from $DATA/w13-golden-m3/{evm,cyclone}.fpchip.gz, the D12 header and the point markers; the sizes of
# the inputs' dumps (*.sram.gz, *.files.gz), and the sram dumps themselves into results if each is at most 8 MB.
# Reads only $DATA/w13-golden-m3 (checked against golden-m3/HASHES first). Writes only results/165-w16-boot-end/
# (run.log and the verdict are the runner's: exit 1 on any failure).
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/165-w16-boot-end; mkdir -p "$RES"
DATA=${T1_DATA:-$HOME/data}; [ -d "$DATA/w13-golden-m3" ] || DATA=$REPO/data
G=$DATA/w13-golden-m3
FAIL=0
{
echo "persistent data: $DATA"; uptime
# sim/tests/golden-m3/HASHES (sim-w16 eb4041a)
cat > /tmp/w16-165-hashes <<'H'
9ffec285e4f9c612157b7e51bc72b4dd82733716c058ebf9c74f2ccb7b999166  evm.fpchip.gz
bf53765324ee4388b64296f241c53508b540083374674b2d86e726cd322ec231  cyclone.fpchip.gz
dcf71fc01b5596c0aea3eb8366942376243eadabfb52ff3d5177639143490d24  evm.files.gz
8317810b72e8b07c25abc62c7f4bbbd4be54e64683e40a72eb9a91abaa40c9db  cyclone.files.gz
64838bcdf9bdfaa29067ccd2bc5a8610e9ba569d0a3415abfa34d466dd28f406  evm.sram.gz
a228f9c394514c92365e2e2bd3c1b1b363f510d9b04b86ca77786bc7b30d6f61  cyclone.sram.gz
H
ls -l "$G"
(cd "$G" && sha256sum -c /tmp/w16-165-hashes) || { echo "the references differ from HASHES"; FAIL=1; }
rm -f /tmp/w16-165-hashes
for S in evm cyclone; do
  echo "== $S"
  zcat "$G/$S.fpchip.gz" | grep '^#' > "$RES/$S.header.txt"
  grep -E '^# (boot-end|cc3-exit|end) ' "$RES/$S.header.txt"
  B=$(sed -n 's/^# boot-end \([0-9]*\)$/\1/p' "$RES/$S.header.txt")
  [ "$B" = 7222 ] || { echo "$S: boot-end is '$B', want 7222"; FAIL=1; }
  zcat "$G/$S.fpchip.gz" | awk -v c="$B" '$1 == c' > "$RES/$S.boot.fp"
  N=$(wc -l < "$RES/$S.boot.fp"); echo "$S: $N lines at cycle $B"
  [ "$N" -ge 1214 ] || { echo "$S: fewer than 1,214 unit lines"; FAIL=1; }
  for f in sram files; do
    Z=$(stat -c %s "$G/$S.$f.gz"); U=$(zcat "$G/$S.$f.gz" | wc -c); echo "$S.$f.gz: $Z bytes, $U uncompressed"
    if [ $f = sram ] && [ "$Z" -le 8388608 ]; then cp "$G/$S.$f.gz" "$RES/"; echo "  copied"; fi
  done
done
gzip -n -9 "$RES"/*.boot.fp
ls -l "$RES"
} 2>&1
exit $FAIL
