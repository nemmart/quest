# Provenance — current artifact checksums (sha256)

Sessions verify the tree they were handed against this table before
starting (METHOD §14 diff-audit).  The IR files carry their own
provenance headers (IR.md §1) and the loader recomputes those at
launch, so they cannot drift silently; this table is for the humans
and the plan-gate checks.  Update it whenever Disassembled/ or a
c_src/ input artifact is regenerated, with the reason.

## Sep 5 2026 — after the Tools fixes (HWFindings_Sep5.md §6, P27 prompt)

Changed (Follow.java XJMP edge; OldDisassembler XCALL `ea,arg` + LNDO/LWDO register):

| file | sha256 (first 16) | was |
|---|---|---|
| Disassembled/quest.dis      | 5c1db5fb75c8c26a | 1f9153c0299cd482 |
| Disassembled/quest-rt.dis   | c62af2273a13cb47 | (changed: 71 XCALL lines) |
| Disassembled/quest.code     | b42d7230546f2934 | (changed: same text) |
| Disassembled/quest.blocks   | 772d21aa51f28290 | (changed: 1,160 XJMP successor lists) |
| Disassembled/quest.tags     | 906598437779a96d | (changed: 1,160 XJMP tag lines) |
| c_src/quest.blocks.split    | 1d3baaf6487a8e3f | a5efa05f59e7af67 |
| c_src/quest.ir2.book        | b510a58bbfa05ba5 | (regenerated; headers + 64 text lines) |
| c_src/quest.ir2.stock       | fa0ab9ba7442b9de | (regenerated) |

Unchanged (verified byte-identical):

| file | sha256 (first 16) |
|---|---|
| Disassembled/quest.targets  | d0ffe4a0963214af |
| Disassembled/quest.addrs    | 59b334186d20ba03 |
| Disassembled/quest.argmap   | 39c42d4c4b749f32 |
| Disassembled/quest.callsites| 73b92c671e94f624 |
| Disassembled/quest.symbols  | 7fc5e4f715d9022b |
| Disassembled/quest.wpsh_wpop| a4a4063374edf7b0 |
| Disassembled/quest.mem      | 1d44317d0995843e |
| c_src/quest.synclist.split  | 42bde6c45c24658c |
| c_src/quest.pushmap.M4      | b89536597005f304 |
| c_src/quest.addrbook        | e6fde2c246630e0e |

Full digests: `sha256sum Disassembled/* Work/c_src/quest.*` on branch
hw-findings-sep5 commit 244d0c8 (Disassembled + regen) — the values
above are prefixes for reading; the files themselves are the record.

## Sep 5 2026 — after P27 (DERR cluster compression, merged bd3369c)

| file | sha256 (first 16) | note |
|---|---|---|
| c_src/quest.synclist.p27    | af1be42f5831fb2c | NEW — the sync list of record for the ir 3 artifacts (13,510 entries) |
| c_src/quest.ir2.book        | 1dc6356a2b45cc06 | regenerated: 2,271 clusters folded, embeds 6,258 |
| c_src/quest.ir2.stock       | d5e4cb13653c0b29 | regenerated |
| c_src/quest.synclist.split  | 42bde6c45c24658c | unchanged (identity; all-emulated runs) |
| Disassembled/*, blocks.split, pushmap, addrbook | — | unchanged from the table above |
