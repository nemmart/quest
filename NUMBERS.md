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
