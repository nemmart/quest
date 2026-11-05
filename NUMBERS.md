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
