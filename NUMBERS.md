# Task numbers

**Claim a number here before writing a task.** Every task in `tasks/` is `NNN-name.sh`, and the runner runs them
oldest-first by number, so two sessions must never pick the same NNN. Git makes the claim atomic:

1. `git pull`, then **append one line** to the table below: the next number after the last line, your session, and a
   few words on the task.
2. Commit (`claim NNN`) and **push before you write the task**.
3. If the push is rejected, or the pull conflicts on that line, someone claimed it first: `git pull` again, take the
   next number after theirs, and push again.

A number in this table is never reused, even if its task is abandoned. Re-runs of a task use a letter suffix on the
same number (`044b-...`), claimed the same way.

## Already used before this registry (2026-11-01)

| numbers | project |
|---|---|
| 001-063 (with 044b, 045b, 047b, 049b) | Quest (the emulator project) |
| 064-068 | meshsorter (v1.1's sort, on the runner) |
| 069 | meshsim WORKER9: the standalone tracer's 2²⁹ `--validate` runs |
| 070-072 | meshsim WORKER8: the v1.2 sort at full size, its per-function watches, run_tests extras |
| 073-076 | meshsim WORKER4: the v1.2 simulator's phase 4 full runs, quick suite, phase 6 |
| 077 | meshsim WORKER4: the v1.2 simulator and sort combined |
| 078 | meshsim WORKER11: the v1.25 replayer's calibration |
| 079-080 | meshsim WORKER9: the v1.25 deliveries, EVM Zone and Cyclone at 2²⁹ |

## Claims

| NNN | session | task |
|---|---|---|
| 081 | meshsim WORKER9 | probe: what the runner sees of the w9 tarballs in /tmp |
| 069b | meshsim WORKER9 | re-run of 069 (the 2^29 --validate runs) once the runner sees /tmp/w9-tracer.tgz |
| 082 | meshsim WORKER9 | probe: /tmp/w9-tracer.tgz present and its SHA-256, before 069b |
| 083 | meshsim WORKER11 | v1.25 acceptance: every window of evm-zone-29 replayed (Stage A, Stage B), from /tmp/w9-deliveries |
| 084 | meshsim WORKER11 | v1.25 acceptance: every window of cyclone-29 replayed (Stage A, Stage B), from /tmp/w9-deliveries |
| 082b | meshsim WORKER9 | re-run of 082 (the w9-tracer.tgz probe) after the chmod |
| 085 | meshsim WORKER10 | v1.25 log densities: log words per row from the w9 deliveries' state records and rows (no replay), both programs |
| 086 | meshsim WORKER11 | v1.25 freeze check: main (0d63132) run_tests quick + full extras with the sort, and the full-size sort p4/uniform-k128 |
| 087 | meshsim WORKER11 | v1.25 CL1 re-verification: main (ba7a97c) quick suite + full extras + p4/uniform-k128, and the selected windows (w9-selected-windows.tgz) replayed |
| 088 | meshsim WORKER15 | v1.2.1 verification: branch v1.2.1 (090fc40) run_tests quick + full extras with the sort, and the full-size sort p4/uniform-k128 |
| 089 | meshsim WORKER11 | v1.25 section 8 (branch s8): run_tests quick + full extras with the sort, and p4/uniform-k128 against 1,360,082 cycles |
| 090 | meshsim WORKER14 | cc-cores (5cd78a2): run_tests quick + full extras (with CcBench), C&C 1-3 as cores behind Config.ccCores, the switch off elsewhere |
| 091 | meshsim WORKER11 | v1.25 section 8e (branch s8): run_tests quick suite + full extras |
| 092 | meshsim WORKER14 | cc-cores (ba34cbf, main 9133397 merged): run_tests quick + p4/uniform-k128 against 1,229,008 cycles |
| 093 | meshsim WORKER11 | v1.25 freeze check: main (922b835) quick suite + full extras + p4/uniform-k128, and the selected windows (w9-selected-windows.tgz) in stages A, B and F |
| 094 | meshsim WORKER11 | v1.25 freeze check, rerun of 093 quick suite + full extras with the controllers firmware built (main 922b835) |
| 095 | meshsim WORKER18 | W5a: build the ACT4 RV64IMAC tests as upstream jolt 17a9459 does (xpack gcc 15.2, Sail, riscv-arch-test a7c9930), pack the ELFs, and run them through upstream's jolt-emu |
| 096 | meshsim WORKER18 | W5b: capture upstream's inline test vectors (jolt 17a9459, six inline crates' cargo test with a harness logging patch) |
| 096b | meshsim WORKER18 | re-run of 096 with --features host per crate, as upstream CI (096 ran 0 tests) |
| 097 | meshsim WORKER20 | v1.26 smoke: convert both w9 deliveries to 68-word records in scratch, replay windows 0-63 of each (A, B, F every 16th; B at 8 KB logs) |
| 098 | meshsim WORKER20 | v1.26 acceptance: every window of evm-zone-29 (converted in scratch) in Stage A and B, F every 64th, 4 KB queues |
| 099 | meshsim WORKER20 | v1.26 acceptance: every window of cyclone-29 (converted in scratch) in Stage A and B, F every 64th, 4 KB queues |
| 100 | meshsim WORKER20 | v1.26 stall control: every window of evm-zone-29 in Stage B with v1.25's 8 KB log queues |
| 101 | meshsim WORKER20 | v1.26 stall control: every window of cyclone-29 in Stage B with v1.25's 8 KB log queues |
| 102 | meshsim WORKER20 | v1.26 suites: branch v1.26-meshsim run_tests quick + full extras with the sort, and p4/uniform-k128 against 1,229,008 cycles |
| 103 | meshsim WORKER20 | regenerate the 2^29 delivery of evm-zone-29 (WORKER9's delivery_writer, as 079) into persistent data, with its selected windows and w9-selected-windows.tgz |
| 104 | meshsim WORKER20 | regenerate the 2^29 delivery of cyclone-29 (as 080) into persistent data, with its selected windows and w9-selected-windows.tgz |
| 097b | meshsim WORKER20 | re-run of 097 (its 3 attempts used up before the deliveries existed), reading them from persistent data (103, 104) |
| 097c | meshsim WORKER20 | the v1.26 smoke again (as 097b) on the model with DECISIONS-v1.26 item 12 (part 0 the working state) |
| 102b | meshsim WORKER20 | the v1.26 suites again (as 102) on the model with DECISIONS-v1.26 item 12 |
| 105 | meshsim WORKER25 | v1.26.1 smoke: convert both w9 deliveries (v1.25) to v1.26.1 records in scratch, replay windows 0-63 of each (A, B, F every 16th), RUN_CONFIG rows | store log | rd_prev |
| 106 | meshsim WORKER25 | v1.26.1 acceptance: every window of evm-zone-29 (converted in scratch) in Stage A and B, F every 64th, compared with 098 |
| 107 | meshsim WORKER25 | v1.26.1 acceptance: every window of cyclone-29 (converted in scratch) in Stage A and B, F every 64th, compared with 099 |
| 108 | meshsim WORKER25 | v1.26.1 suites: branch v1.26.1 run_tests quick + full extras with the sort, and p4/uniform-k128 against 1,229,008 cycles |
| 109 | meshsim WORKER26 | the Tracer's host-side inputs at 2^29 into persistent data ($DATA/w26-jumpmap): gold's segment file, the jump table (map.lines, frame6.bin), initial.pages, per-segment pages written, proof.json; for evm-zone-29 and cyclone-29 |
| 110 | meshsim WORKER22 | B1 T1 at 2^29: the Tracer model on every segment of evm-zone-29 and cyclone-29 in order on one memory, WORKER26's map and initial image (109), every window against w9-deliveries and Stage A, raw and LRU; cycles per row, stalls, shared-cache hit rates, the walk, a separate-map-cache control |
| 110b | meshsim WORKER22 | re-run of 110 (its 3 attempts used): the ideal controller fixed (stale expansion words at a segment start, Cyclone segment 16); T1 also checks every updated cell at each boundary |
| 110c | meshsim WORKER22 | B1 T1 at 2^29, the instruction-fetch stalls broken down (cold or capacity, straddling second halves) and an instruction-cache sweep (128, 256, 1,024 lines), both workloads, raw checks |
| 111 | meshsim WORKER27 | the input pipeline (meshsim inputs-pipeline, wip/inputs/) at 2^29 for evm-zone-29: deliveries at jolt-asic-planning fa59d9a, the v1.26.1 conversion, gold's segment file, jumpmap's outputs, into $DATA/w27-inputs; every checksum against the runner's copies (103, 106, 109); deleted after a pass |
| 112 | meshsim WORKER27 | the same for cyclone-29 (against 104, 107, 109) |
| 110d | meshsim WORKER22 | B1 T1 at 2^29 through milestone 2's output path (queues 0-3, credits, service flags, the drain, 128-bit DMA words): every window and boundary of both workloads, raw, LRU with Stage A, separate-map control; the drain's cycles |
| 110e | meshsim WORKER22 | B1 T2 at 2^29: segment 0 of evm-zone-29 and cyclone-29 on the full chip (C&C 3's tracer_ctl, the Tracer's DMA into the 64 tiles, every tile replaying), raw and LRU; the time per segment |
| 110f | meshsim WORKER22 | B1 T2 at 2^29 with Q15's handshake: evm-zone-29 segments 16, 32, 48, 64, 80 (1 in 16) and the heaviest, 37 and 84, raw, and 37 LRU, on the full chip |
| 110g | meshsim WORKER22 | B1 T2 at 2^29 with Q15's handshake: cyclone-29 segments 16, 32, 48, 64, 80 (1 in 16) and the heaviest, 10 and 14, raw, and 10 LRU, on the full chip |
| 110h | meshsim WORKER22 | B1's full regression on v1.3-tracer: run_tests quick + full extras (the B1 suites included), the full-size sort p4/uniform-k128 against 1,229,008 cycles, the 64-window Stage B smoke of both workloads (as 105) |
| 110i | meshsim WORKER22 | B1 T2 at 2^29, evm-zone-29 segments 16, 32, 37, 48 (110f split): one JVM, the memory carried once, each segment's full chip in a thread |
| 110j | meshsim WORKER22 | B1 T2 at 2^29, evm-zone-29 segments 37 (LRU), 64, 80, 84 (110f split) |
| 110k | meshsim WORKER22 | B1 T2 at 2^29, cyclone-29 segments 10, 10 (LRU), 14, 16 (110g split) |
| 110l | meshsim WORKER22 | B1 T2 at 2^29, cyclone-29 segments 32, 48, 64, 80 (110g split) |
| 110m | meshsim WORKER22 | B1 T2 at 2^29, evm-zone-29 segments 16, 32, 37, 48 (replaces 110i: 'booted' up the request tree, the fan-in that hung 110f and 110i-110l; fail-fast) |
| 110n | meshsim WORKER22 | B1 T2 at 2^29, evm-zone-29 segments 37 (LRU), 64, 80, 84 (replaces 110j) |
| 110o | meshsim WORKER22 | B1 T2 at 2^29, cyclone-29 segments 10, 10 (LRU), 14, 16 (replaces 110k) |
| 110p | meshsim WORKER22 | B1 T2 at 2^29, cyclone-29 segments 32, 48, 64, 80 (replaces 110l) |
| 113 | meshsim WORKER28 | idle-skipping step 1, the profile (branch sim-idle-profile): T2 segment 0 of evm-zone-29 and cyclone-29 raw under JFR (64 windows) and the per-unit probe (32 windows); Stage B and F on windows 0-15 of both under JFR and the probe |
| 113b | meshsim WORKER28 | idle-skipping step 1, the time probe (sim-idle-profile): ns per cycle by chip section, by tile class and child, C&C 3 per unit (the Tracer against its slot and DMA), the cores by state (the interpreter); T2 segment 0 (32 windows) and Stage B/F windows 0-15 of both workloads |
| 113c | meshsim WORKER28 | idle-skipping step 1: Stage B and F on windows 0-15 of both workloads under JFR, the activity probe and the time probe (113 and 113b lacked the tables manifest for Stage B) |
| 110q | meshsim WORKER22 | B1 T2 at 2^29 on the READY/GO barriers (Q15, Q17 as ruled, 7c53209), evm-zone-29 segments 16, 32, 37, 48 |
| 110r | meshsim WORKER22 | B1 T2 at 2^29, READY/GO, evm-zone-29 segments 37 (LRU), 64, 80, 84 |
| 110s | meshsim WORKER22 | B1 T2 at 2^29, READY/GO, cyclone-29 segments 10, 10 (LRU), 14, 16 |
| 110t | meshsim WORKER22 | B1 T2 at 2^29, READY/GO, cyclone-29 segments 32, 48, 64, 80 |
| 110u | meshsim WORKER22 | B1 T2 at 2^29 with C&C 1's firmware owning the segment's barriers (CC_SEGMENT): segment 10 of evm-zone-29 and cyclone-29, every unit's real firmware |
| 114 | meshsim WORKER28 | stage 1 (sim-idle-skip): T2 segment 0 of evm-zone-29 and cyclone-29 (64 windows, raw) and Stage B/F on windows 0-63 of both, stage 1 off and on; every chip's fingerprint, the logs and CSVs compared; us per cycle against 113b's 46.9 |
| 114b | meshsim WORKER28 | stage 1 (sim-idle-skip): the quick suite on and off, every step compared, the sort included; shuffled steps on against unshuffled off |
| 113d | meshsim WORKER28 | the short probe run on WORKER22's READY/GO barriers (sim-idle-profile merged with v1.3-tracer a326fbd): the activity probe on T2 segment 0 of both workloads, first 32 windows |
| 110v | meshsim WORKER22 | B1's final regression at v1.3-tracer 0d023ba (what merges): run_tests quick + full extras, the full-size sort p4/uniform-k128 against 1,229,008 cycles, the 64-window Stage B smoke of both workloads (as 110h) |
| 115 | meshsim WORKER28 | stage 1 on main with B1 merged (sim-idle-skip 3fd4e5f): T2 segment 0 and the 64-window Stage B/F of both workloads, off and on, fingerprints, logs, CSVs, speed |
| 115b | meshsim WORKER28 | stage 1 on main with B1 merged: the quick suite off and on, the sort included; shuffled on against unshuffled off |
| 116 | meshsim WORKER29 | P2 step 1, the profile of a busy chip (sim-step-cost 2ccc514, engine unchanged): the full-size sort p4/uniform-k128, ReplayBench (all 64 tiles replaying, segment 0 of both workloads, 5 repeats) and its synthetic run, T2 segment 0 evm-zone-29 with stage 1 on; each under JFR and plain |
| 116 (duplicate of WORKER29's; WORKER28's baselines) | meshsim WORKER28 | the baselines at GO/READY/error latency 8 (sim-idle-skip, the latency commit e8b18e6): quick and full suites, the full-size sort p4/uniform-k128, the 64-window Stage B smoke (WORKER22's 110v regression, rerun) |
| 116b | meshsim WORKER28 | latency 8: T2 segment 0 of both workloads and Stage B/F windows 0-63, stage 1 off and on (fingerprints, logs, CSVs, speed) |
| 117 | meshsim WORKER28 | stage 1 on main (sim-idle-skip fec569c: main merged forward, GO/READY/error 8, stage_delay 64, the Tracer's counters in the fingerprint): T2 segment 0 and Stage B/F windows 0-63 of both workloads, off and on |
| 117b | meshsim WORKER28 | the same commit: the quick suite off and on, the sort included; shuffled on against unshuffled off |
| 118 | meshsim WORKER28 | the full regression on main 2d8d9c1 (stage 1 merged, GO/READY/error 8, stage_delay 64; stage 1 on): quick and full suites, the full-size sort p4/uniform-k128, the 64-window Stage B smoke, T2 segment 0 of both workloads; the baselines for BASELINES-8.md |
| 119 | meshsim WORKER29 | P2 A's proof on the runner: sim-step-cost 7606a9e (A merged with main) against main 2d8d9c1: the quick suite, the full-size sort, T2 segment 0 and ReplayBench windows 0-63 of both workloads (fingerprints, masked logs, T2's CSVs); then the speed of each alone |
| 120 | meshsim WORKER29 | P2 C's proof on the runner: sim-step-cost b7655df (A + C: overflow sinks, GO queue head, read-port set minimum, direct calls) against main 2d8d9c1: the quick suite, the full-size sort, T2 segment 0 and ReplayBench windows 0-63 of both workloads; then the speed of each alone |
| 121 | meshsim sim/ WORKER1 | the runner probe for sim/ (branch sim-w1): cores, memory, cgroup limits, compilers, the riscv toolchains, Java, perf, huge pages; then make check (gcc and clang, debug with ASan/UBSan and release) from an embedded kit |
| 122 | meshsim sim/ integrator | confirm the owner's runner changes for sim/ (main 7cf9eb3): make check with clang (and gcc) in debug and release; perf record and report on a short run; TSan with gcc and clang (a clean program passes, a racy one is caught); the CPU topology (P- and E-core ids for taskset) |
| 123 | meshsim sim/ WORKER2 | the observer's proof (sim-w2 against main's sim/ref: the quick suite with the observer off and on, masked logs and fingerprints), then the baseline (D8, D13): ReplayBench (sim-step-cost) on segment 0 of evm-zone-29 as 1 JVM on a P-core, 12 JVMs on CPUs 0-11, 16 JVMs on all CPUs |
| 124 | meshsim sim/ WORKER6 | the busy-mesh benchmark (sim-w6 490ab2a, timing): link word two words against three, destination as a word or a parallel array, runtime against fixed widths, ready marks against polling; one mesh on CPU 0, then 128 meshes on 1/6/12 P-core threads and the E-cores; perf stat and record |
| 125 | meshsim sim/ WORKER6 | the Fp128 benchmark (sim-w6 490ab2a, timing; D14, the fold by 159): the fold checked, three forms' latency and throughput under four builds, the toy multiplier unit at 64 and 8192 units on 1/6/12 P-core threads and the E-cores |
| 126 | meshsim sim/ WORKER6 | D15's addition (sim-w6 2a1ccb5, timing): mesh throughput against N (1..12 threads, one mesh each, then 16 each) for (a) and (b); footprint per mesh; ready marks per delivery, none and on transitions at three loads |
| 127 | meshsim sim/ WORKER5 | the controller's speed (sim-w5 8738cfd, timing, clang release, CPU 0): the 67 goldens on the engine and the direct driver; bench_controller (a 13-cycle loop, 10^7 iterations) on the engine untraced and traced, and on the plain core; golden --bench; perf stat; perf record of the plain build and of a UNITS_PROFILE build (the core out of line) |
| 128 | meshsim sim/ WORKER4 | threads, form 1 (sim-w4, timing, clang release): independent chips on independent threads, the framework mesh (W6's 8x8 rebuilt as units) and the synthetic chip; one chip per thread at 1-6 and 12 P-core threads and the E-cores, 16 per thread at 1/6/12, the same 12 chips on 1/6/12 threads (identical digests, perf stat); footprint per chip |
| 129 | meshsim sim/ WORKER5 | M1's threads test (sim-w5 8e19528): N controllers on N threads (run_independent), each a different golden, all 67 in rounds, N = 1, 2, 4, 6, 8, 12 pinned to the P-cores and 16 unpinned; TSan (gcc, clang); the controller's throughput at 1, 6 and 12 P-core threads and on the 4 E-cores (timing) |
| 130 | meshsim sim/ WORKER5 | the framework against ref (sim-w5 b61c8ca, timing, CPU 0): bench_controller's loop in ref through TraceRun's boot with the observer off (SpeedRun), idle-skip on and off, against the framework's engine at the same N; the loop's trace equal in both; JFR of ref folded (JfrFold) |
| 131 | meshsim sim/ WORKER5 | M1's acceptance with the debug checks real (sim-w5 a228eee, main's increment 5 merged): golden --engine (67 x the six option sets), golden --threads N with the six option sets for N = 1, 2, 4, 6, 8, 12 pinned and 16 unpinned; make check (gcc, clang, debug, release, TSan) |
| 132 | meshsim sim/ WORKER3 | the performance pass, before and after in one task (sim-w3: before 0c7df79, after 057a9d5; timing, clang release, pinned to a detected P core, D21: the machine recorded): W5's bench_controller and W4's framework mesh interleaved 5 each, the core alone, footprint per chip, mesh digests, the goldens on the after engine under the six option sets, perf stat and record |
| 133 | meshsim sim/ WORKER4 | threads form 2 (sim-w4, timing, clang release; machine from sim/tools/machine.sh, D21): one framework mesh over 2/4/one-per-P-core threads against one; the barrier spin, yield after 200/2000/20000, block; the due mask's cost; the split's cost on one thread; the barrier at rate 0; digests; footprint |
| 134 | meshsim sim/ WORKER7 | M2's references and baseline (sim-w7 f47b12f, timing first): the full sort p4/uniform-k128 in ref on one JVM pinned to one detected P-core, 3 runs split into setup, boot and the counted run (M2Sort), and one SortTest whole-JVM run against Quest 120's 110 s; then golden-m2's full-size references (the sort's facts, coverage, PhaseLog, fingerprints, traces of tiles 0/27/63) into $DATA with hashes, and regen.sh --check of every local reference |
| 135 | meshsim sim/ WORKER7 | golden-m2 at sim-w7 fb6f21e: the full sort's references again (regen.sh ALL=1 '^sort64k', the .fp with its D12 header) into $DATA/w7-golden-m2/fb6f21e; every file equal to Quest 134's except sort64k-fp.fp, whose hash lines must equal 134's; its new sha256 for HASHES |
| 136 | meshsim sim/ WORKER10 | the speed baseline (sim-w10 4bc4d67 = main's framework, timing, clang release; machine from machine.sh, D21): W4's mesh (rates 0.35, 0.05) and the synthetic chip on one thread with perf; the split's cost on one thread (rows 2-8, tiles 64, synth 8 clusters); form 2 by rows, by tiles and synth over 2/4/P/8/all-P threads; digests; footprint |
| 137 | meshsim sim/ WORKER10 | the cheaper split, before/after (sim-w10: before 45b4cbf = Quest 136's main framework, after 2e016ea: one engine per thread, the Link with depth, the observer, the stop request; timing, clang release, machine from machine.sh, D21): the speed matrix of 136 interleaved for both, plus W4's layout (--engines) on the after; digests equal across both |
| 138 | meshsim sim/ WORKER10 | placement by load against by rows and by tiles (sim-w10 a5c5b0d; timing, clang release, machine from machine.sh, D21): form 2 at 2-12 threads, the mesh at 0.35 and 0.05 and the synthetic chip, placements rows / tiles / load / profile (one-thread calibration); balance, crossings, digests |
| 139 | meshsim sim/ WORKER10 | where a split run's time goes (sim-w10 d1f86dd; timing, clang release, machine from machine.sh, D21): Options::time_threads' phase shares (compute, imbalance, barrier wait, drain, publish, clear) at 2/4/P/8/all-P threads, mesh 0.35 tiles and profile, mesh 0.05, synth; plain runs interleaved for the timer's cost |
| 140 | meshsim sim/ WORKER10 | the drain's prefetch, before/after (sim-w10: before d1f86dd, after a58144b; timing, clang release, machine.sh, D21): Quest 139's breakdown for both, interleaved |
