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

## Sep 5 2026 — after P28 (rt_call decoration, ir 4; branch p28-rt-call)

| file | sha256 (first 16) | note |
|---|---|---|
| c_src/quest.ir2.book        | 294f81d3ef779121 | regenerated ir 4: 987 rt_call, embeds 2,322, 2,273 assert, 11,442 goto |
| c_src/quest.ir2.stock       | 1cf12f3393744fdd | regenerated ir 4: 987 rt_call, embeds 4,201 |
| c_src/quest.synclist.p27    | af1be42f5831fb2c | UNCHANGED — still the list of record (rt_call adds/removes no block) |
| docs/Project28/rt_call.ledger | — | NEW: per-site emitted/refused ledger (987 emitted, 0 refused) |
| Disassembled/*, blocks.split, pushmap, addrbook, tags | — | unchanged from the tables above |

Regeneration command (from Work/c_src; identical inputs, `--rt-slice 3
--leftovers` is the ir 4 artifact of record):

    python3 tools/lower.py --dis ../../Disassembled/quest.dis --blocks quest.blocks.split \
      --pushmap quest.pushmap.M4 --argmap ../../Disassembled/quest.argmap --all [--book] \
      --assumed-foldable ../docs/Project27/assumed-foldable.txt --tags ../../Disassembled/quest.tags \
      --rt-slice 3 --leftovers --out quest.ir2.<book|stock> --rt-census ../docs/Project28/rt_call.ledger

With `--rt-slice 0` and no `--leftovers` the output is byte-identical to
the P27 artifacts above except for the `ir 4` header (P28 regression
check, Sep 5).

## Sep 6 2026 — after P31 (located strings, ir 5; branch p31-located-strings)

| file | sha256 (first 16) | note |
|---|---|---|
| c_src/quest.ir2.book        | 281fd5010ad4e8b3 | regenerated ir 5: 652 string statements, embeds 1,670, 987 rt_call, 2,273 assert, 11,442 goto |
| c_src/quest.ir2.stock       | 1122b9852cd2ab76 | regenerated ir 5: 652 string statements, embeds 3,549 |
| c_src/quest.synclist.p27    | af1be42f5831fb2c | UNCHANGED — still the list of record (no block added or removed; the min diamonds stay) |
| docs/Project31/p31.tsv      | 8fb1cb6d59398d1b | NEW: the per-site string artifact (string_sites.py --p31-tsv; header carries dis/blocks/mem sha256) — lower.py's input |
| docs/Project31/strings.ledger | — | NEW: lower.py's per-site record (652 emitted, 0 refused at slice 3) |
| Disassembled/*, blocks.split, pushmap, addrbook, tags | — | unchanged from the tables above |

Regeneration command (from Work/c_src; `--strings-slice 3` is the ir 5
artifact of record):

    python3 tools/lower.py --dis ../../Disassembled/quest.dis --blocks quest.blocks.split \
      --pushmap quest.pushmap.M4 --argmap ../../Disassembled/quest.argmap --all [--book] \
      --assumed-foldable ../docs/Project27/assumed-foldable.txt --tags ../../Disassembled/quest.tags \
      --rt-slice 3 --leftovers --strings-sites ../docs/Project31/p31.tsv --strings-slice 3 \
      --strings-census ../docs/Project31/strings.ledger --out quest.ir2.<book|stock>

With `--strings-slice 0` the output is byte-identical to the P28
artifacts above except for the `ir 5` header (P31 regression check, Sep
6). The artifact itself is regenerated by (from Work/c_src/tools):

    python3 string_sites.py --dis ../../../Disassembled/quest.dis --blocks ../quest.blocks.split \
      --mem ../../../Disassembled/quest.mem --symbols ../../../Disassembled/quest.symbols \
      --sites ../../docs/Project31/sites.txt --strings /tmp/quest.strings \
      --census ../../docs/Project31/census_raw.txt \
      --p31 ../../docs/Project31/p31.ledger --p31-tsv ../../docs/Project31/p31.tsv

## Sep 6 2026 — Follow.java LJSR-I.GOTO edge fix (Tools, user)

Follow tagged every `LJSR I.GOTO` (26 sites) with a `pc+3` return edge;
I.GOTO never returns (StartStop already excluded it). Regenerated:

| file | sha256 (first 16) | note |
|---|---|---|
| Disassembled/quest.tags   | 010ae3eabc0706d6 | 26 lines lose the pc+3 successor |
| Disassembled/quest.blocks | 65b15fc15b1381df | 26 successor lines; block census unchanged (13,494) |
| c_src/quest.blocks.split  | ba30a84139a7d2f4 | 26 successor lines; synclist.split byte-identical |
| targets / dis / addrs / argmap / synclist.p27 | — | unchanged |

The IR artifacts' `blocks` provenance line must be regenerated (P32's
correction commit does it, together with the 17-statement fix).

## Sep 6 2026 — P32 correction commit (P31 tool defect, docs/Project32/Census.md §1.1; branch p32-append-chains)

| file | sha256 (first 16) | note |
|---|---|---|
| c_src/quest.ir2.book        | 1be3aa29c8b2332a | regenerated ir 5: 649 string statements (17 sites corrected: 3 refuse, 14 re-rendered), embeds 1,673; blocks line = ba30a841… |
| c_src/quest.ir2.stock       | 497c49b47ee85b83 | regenerated ir 5: 649 string statements, embeds 3,552 |
| docs/Project31/p31.tsv      | d09a7063ead0455f | regenerated under the call-aware evaluator (649 EMIT / 127 REFUSE) |
| docs/Project31/p31.ledger, sites.txt, census_raw.txt, strings.ledger | — | regenerated (idiom totals unchanged) |
| docs/Project27/assumed-foldable.txt | 0f442cd5b33178ad | regenerated: content identical (2,271 clusters), tags/blocks sha header follows 09f6593 |
| c_src/quest.synclist.p27    | af1be42f5831fb2c | UNCHANGED |
| Disassembled/*, blocks.split, pushmap, addrbook | — | as the 09f6593 table |

Regeneration: the P31 commands above with the current inputs (the
string_sites.py unchanged in its P31 arguments). K=1 gate on the regenerated
artifacts: book k1fo 0 div / 308,923 pairs / clean; stock k1fo-st 0 div
/ 299,387 pairs / clean (local, Sep 6).
