#!/bin/bash
# Task 168-w16-fpchip -- meshsim sim/ WORKER16 (branch sim-w16). Not timing; a minute.
# Every unit's line at every point of M3's references (AUDIT-M3 7: every 100,000 cycles, the boot's end, C&C 3's exit,
# the end): $DATA/w13-golden-m3/{evm,cyclone}.fpchip.gz, checked against sim/tests/golden-m3/HASHES, copied into results
# for the framework's per-unit point checks of the segment. Reads only $DATA/w13-golden-m3; writes only results/168-w16-fpchip/.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/168-w16-fpchip; mkdir -p "$RES"
DATA=${T1_DATA:-$HOME/data}; [ -d "$DATA/w13-golden-m3" ] || DATA=$REPO/data
G=$DATA/w13-golden-m3
echo "persistent data: $DATA"; uptime
cat > /tmp/w16-168-hashes <<'H'
9ffec285e4f9c612157b7e51bc72b4dd82733716c058ebf9c74f2ccb7b999166  evm.fpchip.gz
bf53765324ee4388b64296f241c53508b540083374674b2d86e726cd322ec231  cyclone.fpchip.gz
H
(cd "$G" && sha256sum -c /tmp/w16-168-hashes) || { echo "the dumps differ from HASHES"; exit 1; }
rm -f /tmp/w16-168-hashes
for S in evm cyclone; do
  Z=$(stat -c %s "$G/$S.fpchip.gz"); echo "$S.fpchip.gz: $Z bytes"
  [ "$Z" -le 52428800 ] || { echo "$S: larger than 50 MB, not copied"; exit 1; }
  cp "$G/$S.fpchip.gz" "$RES/"
done
ls -l "$RES"
