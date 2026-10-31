#!/bin/bash
# Task 079 -- meshsim WORKER9 part 3: the full-program replay delivery of cyclone-29 (jolt-cpp 821e1bd), written by
# delivery_writer (jolt-asic-planning tracer-standalone-main 5ba7f0c: the box's tarball at 121c271 plus the imm_signed
# patch below) and proven window by window by its functional replay from the delivered files (all three variants).
# Outputs stay on the box (nothing large is committed):
#   /tmp/w9-deliveries/            a replay-delivery root (manifest.json + one directory per program, tables included)
#   /tmp/w9-deliveries-selected/   the selected windows, same layout; /tmp/w9-selected-windows.tgz is it packed
# results/ gets the logs, sizes, run time and the manifests' SHA-256. Input: /tmp/w9-delivery.tgz (by hand, SHA-256 checked).
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/079-w9-delivery-cyclone-29; mkdir -p "$RES"
AV=$(df -BG --output=avail /tmp | tail -1 | tr -dc 0-9); echo "free in /tmp: ${AV} GB"
[ "$AV" -ge 40 ] || { echo "need about 31 GB for the delivery plus margin; only ${AV} GB free"; exit 4; }
SRC=/tmp/w9-delivery.tgz; WANT=4000c1acf70e6d3795a345b592fa7d7987a23e4230631e456c8d472c3da1ce10
[ -f "$SRC" ] || { echo "missing $SRC"; exit 2; }
GOT=$(sha256sum "$SRC" | cut -d' ' -f1); echo "sha256 $GOT"; [ "$GOT" = "$WANT" ] || { echo "sha256 mismatch"; exit 2; }
W=/tmp/w9-079; rm -rf "$W"; mkdir -p "$W"; tar -xzf "$SRC" -C "$W" || exit 2
cd "$W/w9" || exit 2
patch -p1 > "$RES/patch.log" 2>&1 <<'PATCH_EOF' || { cat "$RES/patch.log"; exit 2; }
diff --git a/tracer/delivery/delivery_writer.cpp b/tracer/delivery/delivery_writer.cpp
index acd6d11..2073f7a 100644
--- a/tracer/delivery/delivery_writer.cpp
+++ b/tracer/delivery/delivery_writer.cpp
@@ -109,10 +109,11 @@ struct Tables {
 // ---------------------------------------------------------------- records (spec 4.19.12)
 using Rec = std::array<u64, 10>;
 Rec make_rec(u32 idx, u8 op, u8 rdr, u8 r1r, u8 r2r, u16 seq, bool comp, bool virt, bool first, bool k0, bool k1, bool k2,
-             u64 pc, u64 imm, u64 rs1, u64 rs2, u64 rdp, u64 rd, u64 maddr, u64 mval) {
+             bool imm_signed, u64 pc, u64 imm, u64 rs1, u64 rs2, u64 rdp, u64 rd, u64 maddr, u64 mval) {
   Rec r{};
   r[0] = u64(idx) | u64(op) << 32 | u64(rdr) << 40 | u64(r1r) << 48 | u64(r2r) << 56;
-  r[1] = u64(seq) | u64(comp) << 16 | u64(virt) << 17 | u64(first) << 18 | u64(k0) << 19 | u64(k1) << 20 | u64(k2) << 21;
+  r[1] = u64(seq) | u64(comp) << 16 | u64(virt) << 17 | u64(first) << 18 | u64(k0) << 19 | u64(k1) << 20 | u64(k2) << 21 |
+         u64(imm_signed) << 22;                                   // word 0 [86], v1.25 4.19.12
   r[2] = pc; r[3] = imm; r[4] = rs1; r[5] = rs2; r[6] = rdp; r[7] = rd; r[8] = maddr; r[9] = mval;
   return r;
 }
@@ -224,9 +225,9 @@ struct Replay {
         else if (isa::is_store(op)) { maddr = isa::memory_address(op, a, x.imm); mval = pre; }
         Rec got = make_rec(idx, x.op, x.rdi ? 0 : x.rd, x.rs1i ? 0 : x.rs1, x.rs2i ? 0 : x.rs2, x.seq,
                            last && compressed && report_c, x.virt, x.first, x.rdi, x.rs1i, x.rs2i,
-                           pc, (u64)x.imm, x.rs1i ? 0 : a, x.rs2i ? 0 : b, cprev, rd_after, maddr, mval);
+                           x.sign /* the expansion word's sign_imm */, pc, (u64)x.imm, x.rs1i ? 0 : a, x.rs2i ? 0 : b, cprev, rd_after, maddr, mval);
         if (got != ref[out]) {
-          static const char *nm[10] = {"word0", "word0hi", "pc", "imm", "rs1", "rs2", "rd_prev", "rd", "mem_addr", "mem_value"};
+          static const char *nm[10] = {"word0", "word0hi (seq, flags, kinds, imm_signed)", "pc", "imm", "rs1", "rs2", "rd_prev", "rd", "mem_addr", "mem_value"};
           std::string d; for (int k = 0; k < 10; ++k) if (got[k] != ref[out][k]) { char bb[96]; std::snprintf(bb, 96, " %s replay %llx ref %llx;", nm[k], (unsigned long long)got[k], (unsigned long long)ref[out][k]); d += bb; }
           return fail(std::string("record differs (") + isa::opcode_to_string(op) + "):" + d);
         }
@@ -394,7 +395,7 @@ int main(int argc, char **argv) {
         const bool load = isa::is_load(op), store = isa::is_store(op);
         Wn->recs.push_back(make_rec(ix, (u8)op, r.rd_reg, r.rs1_reg, r.rs2_reg, vi.virt_seq_remaining, r.is_compressed,
                                     r.is_virtual(), r.is_first_in_sequence(), vi.rd_is_imm, vi.rs1_is_imm, vi.rs2_is_imm,
-                                    r.pc, (u64)r._imm, r.rs1, r.rs2, r.rd_prev, r.rd,
+                                    isa::signed_immediate(op) /* jolt-cpp's own list */, r.pc, (u64)r._imm, r.rs1, r.rs2, r.rd_prev, r.rd,
                                     (load || store) ? r.mem_access_addr : 0, load ? r.rd : store ? r.mem_pre_value : 0));
       }
       if (!vi.rd_is_imm && r.rd_reg) cur.regs[r.rd_reg] = r.rd;
@@ -404,7 +405,7 @@ int main(int argc, char **argv) {
   if (pending) { pending->end = cur; finish(*pending); }
   // manifest
   std::ofstream m(out_dir + "/" + name + ".manifest.json");
-  m << "{\n  \"format_version\": 2,\n  \"jolt_cpp\": \"821e1bd2f433d6ca62300afbbbba031db50e7dc9\",\n  \"writer\": \"jolt-asic-planning PR #2 tracer/delivery/delivery_writer.cpp\",\n"
+  m << "{\n  \"format_version\": 3,\n  \"jolt_cpp\": \"821e1bd2f433d6ca62300afbbbba031db50e7dc9\",\n  \"writer\": \"jolt-asic-planning PR #2 tracer/delivery/delivery_writer.cpp\",\n"
     << "  \"programs\": [{\"name\": \"" << name << "\", \"program_dir\": \"" << prog_dir << "\", \"input_dir\": \"" << input_dir << "\", \"window_rows\": " << W
     << ", \"num_cycles\": " << N << ", \"padded_cycles\": " << P << ", \"window_entries\": [" << manifest_windows << "\n  ]}]\n}\n";
   std::printf("%s: %llu windows replayed OK, %llu failed; %llu rows replayed (3 variants)\n", name.c_str(), (unsigned long long)ok_windows, (unsigned long long)bad_windows, (unsigned long long)rows_replayed);
PATCH_EOF
cd tracer
echo "start $(date -u) on $(hostname), $(nproc) threads"
EXTRA=(); INC=""
if [ ! -e /usr/include/libelf.h ] && [ ! -e /usr/include/libelf/libelf.h ]; then
  EXTRA=(-DCMAKE_PREFIX_PATH="$W/w9/libelf/usr" -DCMAKE_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu")
  export LD_LIBRARY_PATH="$W/w9/libelf/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
  INC="-I$W/w9/libelf/usr/include -L$W/w9/libelf/usr/lib/x86_64-linux-gnu"
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release "${EXTRA[@]}" > "$RES/configure.log" 2>&1 || { tail -30 "$RES/configure.log"; exit 3; }
cmake --build build -j8 --target tracer > "$RES/build.log" 2>&1 || { tail -30 "$RES/build.log"; exit 3; }
sed -i "s#-lelf#$INC -lelf#" delivery/build.sh
bash delivery/build.sh "$PWD" "$PWD/build" "$W/dw" >> "$RES/build.log" 2>&1 || { tail -30 "$RES/build.log"; exit 3; }
TAB="$W/w9/tables"
merge() {  # a replay-delivery root: manifest.json listing every <name>.manifest.json's program; tables in each program dir
  python3 - "$1" <<'PY'
import json, glob, os, sys
root = sys.argv[1]; progs = []
for f in sorted(glob.glob(os.path.join(root, '*.manifest.json'))):
    m = json.load(open(f)); progs += m['programs']; base = m
base['programs'] = progs
json.dump(base, open(os.path.join(root, 'manifest.json'), 'w'), indent=1)
print('root manifest:', [p['name'] for p in progs])
PY
}
OUT=/tmp/w9-deliveries; mkdir -p "$OUT"; rm -rf "$OUT/cyclone-29" "$OUT/cyclone-29.manifest.json"
T0=$(date +%s)
"$W/dw" cyclone cyclone/input/29/input-15961266 "$TAB" "$OUT" cyclone-29 --windows=all --keep=all > "$RES/delivery.log" 2>&1; RC=$?
echo "full delivery: delivery_writer exit $RC after $(( $(date +%s) - T0 )) s"
grep -v INFO "$RES/delivery.log" | tail -4
cp "$TAB"/*.bin "$OUT/cyclone-29/"; merge "$OUT"
du -sb "$OUT/cyclone-29" | tee "$RES/size.txt"; du -sh "$OUT/cyclone-29" | tee -a "$RES/size.txt"
sha256sum "$OUT/cyclone-29.manifest.json" "$OUT/manifest.json" | tee "$RES/manifest.sha256"
python3 -c "import json;W=json.load(open('$OUT/cyclone-29.manifest.json'))['programs'][0]['window_entries'];print(len(W),'windows,',sum(w['replay']=='ok' for w in W),'replayed ok')" | tee "$RES/summary.txt"
# the selected windows
SELD=/tmp/w9-deliveries-selected; mkdir -p "$SELD"; rm -rf "$SELD/cyclone-29" "$SELD/cyclone-29.manifest.json"
"$W/dw" cyclone cyclone/input/29/input-15961266 "$TAB" "$SELD" cyclone-29 --windows=0,1,2,4,964,965,988,1295,4803,5222,5669,5670,8191 --keep=all > "$RES/selected.log" 2>&1 || RC=5
grep -v INFO "$RES/selected.log" | tail -2
cp "$TAB"/*.bin "$SELD/cyclone-29/"; merge "$SELD"
tar -czf /tmp/w9-selected-windows.tgz -C /tmp w9-deliveries-selected && sha256sum /tmp/w9-selected-windows.tgz | tee -a "$RES/manifest.sha256"
df -h /tmp | tail -1
echo "end $(date -u), exit $RC"
exit $RC
