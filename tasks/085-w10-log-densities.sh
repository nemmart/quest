#!/bin/bash
# Task 085-w10-log-densities -- meshsim WORKER10: v1.25's run-log densities (machine-spec 4.19.2, item 138) from the w9
# deliveries in /tmp/w9-deliveries, with no replay. Per window it reads the raw state record's stream word (the exact raw
# read-log and write-log lengths in bits), the LRU record's (compressed lengths) and the reference rows, classifies each
# row's log word as delivery_writer.cpp does (load, store, advice, a transfer to a new pc), and checks the sums against
# the state record. LogDensity.java is embedded below as plain text after the final exit, SHA-256 checked, and run with
# java's single-file launcher. Commits only results (two CSVs, the summary in run.log); reads the deliveries, writes nothing there.
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/085-w10-log-densities; mkdir -p "$RES"
W=/tmp/w10-085; rm -rf "$W"; mkdir -p "$W"
WANT=725e9f6fcc956a90facffc1275d39efc8d889f52c1c1c6e7f94dbd433731f4b4
sed -n '/^__LOGDENSITY_BEGIN__$/,/^__LOGDENSITY_END__$/p' "$0" | sed '1d;$d' > "$W/LogDensity.java" || exit 2
GOT=$(sha256sum "$W/LogDensity.java" | cut -d' ' -f1); echo "LogDensity.java sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "source sha256 mismatch (want $WANT)"; exit 2; }
D=/tmp/w9-deliveries
echo "start $(date -u) on $(hostname), $(nproc) threads"; java -version 2>&1 | head -1
RC=0
for P in "evm-zone-29 7142e894c80a0c9d491f21c852a942885e0abd97cfe6aefd0101d716824e138f" "cyclone-29 f8a0622e144a944c51386b5b36f81101dcdeb16e5a8d950c1e9ca0e3097f8982"; do
  set -- $P; PROG=$1; MSHA=$2
  M=$(sha256sum "$D/$PROG.manifest.json" 2>/dev/null | cut -d' ' -f1)
  if [ "$M" != "$MSHA" ]; then echo "$PROG: manifest sha256 '$M' is not $MSHA: skipped"; RC=1; continue; fi
  echo "$PROG: manifest sha256 ok; reading 8192 windows at $(date -u +%T)"
  ( cd "$W" && java -Xmx8g LogDensity.java "$D" "$PROG" 8192 "$RES/$PROG.csv" ) || RC=1
  echo "$PROG: done at $(date -u +%T)"
done
echo "exit $RC at $(date -u)"
rm -rf "$W"
exit $RC
__LOGDENSITY_BEGIN__
// Quest 085 (meshsim WORKER10): v1.25 log densities from the w9 deliveries, no replay.
// Per window it reads the raw state record (window_NNNNN.state.bin: stream word = exact raw lengths in bits), the
// LRU state record (window_NNNNN.state-lru.bin: compressed lengths) and the reference rows (window_NNNNN.rows.bin,
// 80 bytes per row, spec 4.19.12), classifies every row's log word exactly as the delivery writer does
// (delivery_writer.cpp: is_load -> read, VirtualAdvice -> read, is_store -> write, a transfer whose new pc differs
// from its pc -> read), and checks the per-window sums against the state record's raw lengths.
import java.io.*; import java.nio.*; import java.nio.file.*; import java.util.*; import java.util.concurrent.atomic.*;
import java.util.stream.*;

public class LogDensity {
    // opcodes at jolt-cpp 821e1bd (wip/tables/opcode_table.txt)
    static boolean isLoad(int o) { return o == 54 || (o >= 110 && o <= 112); }
    static boolean isStore(int o) { return o == 53 || (o >= 113 && o <= 115); }
    static final int ADVICE = 126, BEQ = 28, BNE = 29, BLT = 30, BGE = 31, BLTU = 32, BGEU = 33, JAL = 34, JALR = 35;
    static final int[] SPANS = {512, 1024, 4096};

    static long newPc(int op, long pc, long rs1, long rs2, long imm) {
        switch (op) {
            case BEQ: return rs1 == rs2 ? pc + imm : pc;
            case BNE: return rs1 != rs2 ? pc + imm : pc;
            case BLT: return rs1 < rs2 ? pc + imm : pc;
            case BGE: return rs1 >= rs2 ? pc + imm : pc;
            case BLTU: return Long.compareUnsigned(rs1, rs2) < 0 ? pc + imm : pc;
            case BGEU: return Long.compareUnsigned(rs1, rs2) >= 0 ? pc + imm : pc;
            case JAL: return imm == 0 ? 0 : pc + imm;
            case JALR: return (rs1 + imm) & ~1L;
            default: return pc;
        }
    }

    record W(int index, long real, long loads, long stores, long advice, long nonseq, long rawR, long rawW,
             long lruR, long lruW, boolean rawFlags, int[] maxR, int[] maxW, int[] maxT, String err) {}

    static W one(Path dir, int k) {
        String base = String.format("window_%05d.", k);
        try {
            ByteBuffer st = ByteBuffer.wrap(Files.readAllBytes(dir.resolve(base + "state.bin"))).order(ByteOrder.LITTLE_ENDIAN);
            ByteBuffer lru = ByteBuffer.wrap(Files.readAllBytes(dir.resolve(base + "state-lru.bin"))).order(ByteOrder.LITTLE_ENDIAN);
            ByteBuffer rb = ByteBuffer.wrap(Files.readAllBytes(dir.resolve(base + "rows.bin"))).order(ByteOrder.LITTLE_ENDIAN);
            if (st.capacity() != 1072 || lru.capacity() != 1072 || rb.capacity() % 80 != 0) return err(k, "bad file size");
            boolean rawFlags = ((st.getLong(16 + 8) >>> 48) & 7) == 0;          // state word [114:112]
            long sw0 = st.getLong(32), sw1 = st.getLong(40), lw0 = lru.getLong(32), lw1 = lru.getLong(40);
            long rawR = (sw0 >>> 32) & 0xFFFFFF, rawW = sw1 & 0xFFFFFF, lruR = (lw0 >>> 32) & 0xFFFFFF, lruW = lw1 & 0xFFFFFF;
            int n = rb.capacity() / 80;
            byte[] r = new byte[n], w = new byte[n];
            long real = 0, loads = 0, stores = 0, advice = 0, nonseq = 0;
            for (int i = 0; i < n; i++) {
                int b = 80 * i;
                long w0 = rb.getLong(b);
                int op = (int) ((w0 >>> 32) & 0xFF);
                long pc = rb.getLong(b + 16), imm = rb.getLong(b + 24), rs1 = rb.getLong(b + 32), rs2 = rb.getLong(b + 40);
                boolean pad = op == 0 && pc == 0 && (w0 & 0xFFFFFFFFL) == 0;
                if (!pad) real++;
                if (isLoad(op)) { loads++; r[i] = 1; }
                else if (op == ADVICE) { advice++; r[i] = 1; }
                else if (isStore(op)) { stores++; w[i] = 1; }
                if (op >= BEQ && op <= JALR && newPc(op, pc, rs1, rs2, imm) != pc) { nonseq++; r[i]++; }
            }
            int[] mR = spans(r, null), mW = spans(w, null), mT = spans(r, w);
            String e = "";
            if (rawR % 64 != 0 || rawW % 64 != 0) e += "raw length not whole words; ";
            if (loads + advice + nonseq != rawR / 64) e += "read words " + (loads + advice + nonseq) + " vs state " + rawR / 64 + "; ";
            if (stores != rawW / 64) e += "write words " + stores + " vs state " + rawW / 64 + "; ";
            if (!rawFlags) e += "state.bin flags not raw; ";
            return new W(k, real, loads, stores, advice, nonseq, rawR, rawW, lruR, lruW, rawFlags, mR, mW, mT, e);
        } catch (IOException x) { return err(k, x.toString()); }
    }
    static W err(int k, String e) { return new W(k, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, new int[3], new int[3], new int[3], e); }

    // the most log words in any span of SPANS[j] consecutive rows of the window (spans wholly inside it)
    static int[] spans(byte[] a, byte[] b) {
        int n = a.length; int[] m = new int[SPANS.length];
        for (int j = 0; j < SPANS.length; j++) {
            int L = Math.min(SPANS[j], n), s = 0, best = 0;
            for (int i = 0; i < n; i++) {
                s += a[i] + (b == null ? 0 : b[i]);
                if (i >= L) s -= a[i - L] + (b == null ? 0 : b[i - L]);
                if (i >= L - 1 && s > best) best = s;
            }
            m[j] = best;
        }
        return m;
    }

    static double med(double[] v) { double[] c = v.clone(); Arrays.sort(c); return c.length == 0 ? 0 : c[c.length / 2]; }

    public static void main(String[] a) throws Exception {
        Path root = Paths.get(a[0]); String prog = a[1]; int nw = Integer.parseInt(a[2]); Path out = Paths.get(a[3]);
        Path dir = root.resolve(prog);
        AtomicInteger done = new AtomicInteger();
        List<W> ws = IntStream.range(0, nw).parallel().mapToObj(k -> {
            W x = one(dir, k); int d = done.incrementAndGet();
            if (d % 1024 == 0) System.err.println(prog + ": " + d + " windows read");
            return x; }).sorted(Comparator.comparingInt(W::index)).collect(Collectors.toList());
        try (PrintWriter p = new PrintWriter(Files.newBufferedWriter(out))) {
            p.println("program,window,real_rows,loads,stores,advice,nonseq,raw_rlog_bits,raw_wlog_bits,lru_rlog_bits,lru_wlog_bits,maxR512,maxR1024,maxR4096,maxW512,maxW1024,maxW4096,maxT512,maxT1024,maxT4096,check");
            for (W x : ws) p.printf("%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s%n", prog, x.index, x.real, x.loads, x.stores, x.advice, x.nonseq,
                    x.rawR, x.rawW, x.lruR, x.lruW, x.maxR[0], x.maxR[1], x.maxR[2], x.maxW[0], x.maxW[1], x.maxW[2], x.maxT[0], x.maxT[1], x.maxT[2], x.err.isEmpty() ? "ok" : "\"" + x.err + "\"");
        }
        long bad = ws.stream().filter(x -> !x.err.isEmpty()).count();
        long real = 0, L = 0, S = 0, A = 0, N = 0, rawBits = 0, pickBits = 0;
        int[] wR = new int[3], wW = new int[3], wT = new int[3];
        List<Double> full = new ArrayList<>();
        int rows = 65536;
        for (W x : ws) {
            real += x.real; L += x.loads; S += x.stores; A += x.advice; N += x.nonseq;
            rawBits += x.rawR + x.rawW; pickBits += Math.min(x.rawR, x.lruR) + Math.min(x.rawW, x.lruW);
            for (int j = 0; j < 3; j++) { wR[j] = Math.max(wR[j], x.maxR[j]); wW[j] = Math.max(wW[j], x.maxW[j]); wT[j] = Math.max(wT[j], x.maxT[j]); }
            if (x.real == rows) full.add((double) (x.loads + x.stores + x.advice + x.nonseq) / rows);
        }
        double[] f = full.stream().mapToDouble(d -> d).toArray();
        System.out.printf("%s: %d windows, %d checked against the state records, %d mismatches or errors%n", prog, ws.size(), ws.size() - bad, bad);
        System.out.printf("%s: real rows %d; log words: loads %d, stores %d, advice %d, non-sequential %d%n", prog, real, L, S, A, N);
        System.out.printf("%s: density per real row: load %.4f / store %.4f / advice %.5f / non-sequential %.4f, total %.4f (read log %.4f, write log %.4f)%n",
                prog, (double) L / real, (double) S / real, (double) A / real, (double) N / real, (double) (L + S + A + N) / real, (double) (L + A + N) / real, (double) S / real);
        System.out.printf("%s: per full real window (%d): total density median %.4f, min %.4f, max %.4f%n", prog, f.length, med(f),
                Arrays.stream(f).min().orElse(0), Arrays.stream(f).max().orElse(0));
        for (int j = 0; j < 3; j++) System.out.printf("%s: worst %d-row span: total %.4f, read %.4f, write %.4f words per row%n", prog, SPANS[j],
                (double) wT[j] / SPANS[j], (double) wR[j] / SPANS[j], (double) wW[j] / SPANS[j]);
        System.out.printf("%s: log bytes, all windows: raw %.1f MB, each stream's smaller form %.1f MB; per 64-window pass (%d passes) %.2f MB raw, %.2f MB smaller form%n",
                prog, rawBits / 8e6, pickBits / 8e6, ws.size() / 64, rawBits / 8e6 / (ws.size() / 64.0), pickBits / 8e6 / (ws.size() / 64.0));
        System.exit(bad == 0 ? 0 : 1);
    }
}
__LOGDENSITY_END__
