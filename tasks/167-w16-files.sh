#!/bin/bash
# Task 167-w16-files -- meshsim sim/ WORKER16 (branch sim-w16). Not timing; a minute.
# The memory at segment 10's start of M3's references (M3Inputs.files: the FileStore T1's carry leaves; AUDIT-M3 7):
# $DATA/w13-golden-m3/{evm,cyclone}.files.gz, checked against sim/tests/golden-m3/HASHES, copied into results for the
# framework's boot-end check (tr.files). Reads only $DATA/w13-golden-m3; writes only results/167-w16-files/.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/167-w16-files; mkdir -p "$RES"
DATA=${T1_DATA:-$HOME/data}; [ -d "$DATA/w13-golden-m3" ] || DATA=$REPO/data
G=$DATA/w13-golden-m3
echo "persistent data: $DATA"; uptime
cat > /tmp/w16-167-hashes <<'H'
dcf71fc01b5596c0aea3eb8366942376243eadabfb52ff3d5177639143490d24  evm.files.gz
8317810b72e8b07c25abc62c7f4bbbd4be54e64683e40a72eb9a91abaa40c9db  cyclone.files.gz
H
(cd "$G" && sha256sum -c /tmp/w16-167-hashes) || { echo "the dumps differ from HASHES"; exit 1; }
rm -f /tmp/w16-167-hashes
for S in evm cyclone; do
  Z=$(stat -c %s "$G/$S.files.gz"); echo "$S.files.gz: $Z bytes"
  [ "$Z" -le 52428800 ] || { echo "$S: larger than 50 MB, not copied"; exit 1; }
  cp "$G/$S.files.gz" "$RES/"
done
ls -l "$RES"
