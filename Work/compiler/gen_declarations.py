#!/usr/bin/env python3
"""gen_declarations.py — GENERATE game/declarations.h and game/declarations.json.

The world layout as the reconstructed source sees it: the three shared-
memory base pointers (SD_PTR, OBJ_PTR, CAS_PTR), the key globals, and the
record tables the P35 routines touch.  Sources: docs/GAME_REFERENCE.md
(addresses, names) + docs/Project34/Census.md §3 (strides, raw field
displacements K, the 1-based origin rule origin = minK + stride).

Field names are `f<K>` (K = the raw word displacement from the base, as
readable.py names them) unless a meaning is known.  The .h is the ONLY
game file whose C and C++ views differ (game/README.md); the .json is the
same table for compiler/translate.py, which needs the numbers, not types.

    python3 compiler/gen_declarations.py [--out game/]
"""
import argparse, hashlib, json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))

# ---------------------------------------------------------------------------
# THE TABLE (hand-seeded; every row cites its source)
# ---------------------------------------------------------------------------

# static scalars: name -> (word address, width in bits, comment)
STATICS = {
    "SD_PTR":     (0x70000210, 32, "GAME_REFERENCE: player records, world state (pointer)"),
    "OBJ_PTR":    (0x70000212, 32, "GAME_REFERENCE: object/item placement data (pointer)"),
    "CAS_PTR":    (0x70000214, 32, "GAME_REFERENCE: castle data (pointer)"),
    "PLAYER_NUM": (0x70000216, 16, "GAME_REFERENCE: current player index"),
    "OUT_CHAN":   (0x70000260, 32, "GAME_REFERENCE: output channel (?WRITE_SCREEN arg 1)"),
    "IN_CHAN":    (0x70000262, 32, "GAME_REFERENCE: input channel"),
    "CITY_NUM":   (0x7000026C, 16, "GAME_REFERENCE"),
    "TERR_NUM":   (0x7000026E, 16, "GAME_REFERENCE"),
    "MOVES_LEFT": (0x70000270, 16, "GAME_REFERENCE"),
}

# direct fields through a base pointer: pointer -> {name: (K, width, comment)}
DIRECT = {
    "SD_PTR": {
        "seed":        (40, 32, "P34 Census §3: the ?RANDOM_NUMBER seed (SD_PTR->f40)"),
        "f42":         (42, 16, "P34 Census §3"),
        "player_count": (43, 16, "UPDATE_SCREENS loop bound (SD_PTR->f43.h)"),
    },
    "OBJ_PTR": {
        "region_count": (11502, 32, "P34 Census §3 / PICK_X_Y_reading: region count (OBJ_PTR->f11502)"),
    },
}

# record tables: name -> dict(base pointer, stride, minK (field 0's raw K), bound, fields{name:(K,width)})
# origin = minK + stride (P34 origin rule); element i (1-based) at base + i*stride + K.
TABLES = {
    "REGION": dict(base="OBJ_PTR", stride=9, minK=11495, bound=100000,
                   comment="P34 Census §3 OBJ_PTR_rec9; bound 100000 = PICK_X_Y's DERR 17 check",
                   fields={"x": (11495, 16), "y": (11496, 16), "type": (11497, 16),
                           "f11498": (11498, 32), "f11500": (11500, 32),
                           "f11502": (11502, 16), "f11503": (11503, 16)}),
    "PLAYER": dict(base="SD_PTR", stride=686, minK=-686 + 0, bound=10,
                   comment="P34 Census §3 SD_PTR_rec686; bound 10 = the DERR 17 check on PLAYER_NUM; "
                           "K < 0 in this table are raw displacements (origin unknown: min K seen −642)",
                   fields={"fm589": (-589, 16), "fm588": (-588, 16),
                           "screen": (-611, 32, (9, 11), (22, 2)),   # UPDATE_SCREENS: 9 rows x 11 cells of 2 words, raw K -611
                           "f19": (19, 32), "fm625": (-625, 16), "fm608": (-608, 16),
                           "fm590": (-590, 16),   # DIED: bit 4 of this word is the first test (bit address 16*i*686-9436)
                           "fm591": (-591, 16),   # P37/OWNS + DIED: the second bit word (bits 14, 15 seen in OWNS)
                           # P37/OWNS 70175CDB: PLAYER(p).fm390(i), 10 x 16-bit, inner stride 1 word,
                           # 1-based (the DERR 17 bound on i is 10, as it is on p)
                           # P39/LIST_PLAYERS.3 7016F59F: XNLDA 0,[ac2+0x7D8B]
                           "fm629": (-629, 16),
                           "fm390": (-390, 16, (10,), (1,))}),
}


# ---------------------------------------------------------------------------
# PARENT FRAMES — the static link's target layout (P39)
# ---------------------------------------------------------------------------
# A nested procedure reaches its enclosing procedure's variables through the
# link at wp(fp, -6).  To emit the displacement the translator needs the
# PARENT's frame layout, which is not known from the parent's own source (no
# parent is reconstructed yet) — so it is recorded here, slot by slot, as the
# nested procedures witness it.
#
# USER RULING (P39 gate, Sep 9 2026) — a slot enters this table ONLY with a
# recorded WIDTH and a NAMED WITNESS.  Sibling nested procedures must agree on
# their common parent's layout INDEPENDENTLY; a disagreement is a finding, not
# something to reconcile.  That mutual check is the difference between deriving
# the parent's frame and fitting it.  `check_frames()` below enforces the
# mechanical half (width + witness present); the independence is procedural.
#
# name -> dict(argc, args{k: (width, witness, comment)}, locals{name: (slot, width, witness, comment)})
# `args` carries the parent's OWN parameters, reached as UPARG(P, k): they are
# by-reference, so the width is the width of the DATUM, not of the slot.  They
# obey the same witness rule as the locals.
# Slot numbers are WORD displacements off the parent's frame pointer, exactly
# as they appear in the IR: `wp(link, slot)`.  Locals are named `w<slot>` when
# the meaning is not known — the same convention readable.py uses for fields.
PARENT_FRAMES = {
    "LIST_PLAYERS": dict(
        argc=0, args={},
        locals={
            "w13": (13, 16, "LIST_PLAYERS.3@7016F59F",
                    "XNLDA 1,[ac2+0xD] — compared against PLAYER(i).fm629"),
        }),
    "FIRE": dict(
        argc=2,
        args={1: (16, "FIRE.2@7016A479",
                  "XNLDA 1,@[ac2+0xFFF4] — a PLAYER subscript (DERR 17 bound 10)")},
        locals={
            "w10": (10, 32, "FIRE.1@7016A3C0",
                    "XWLDA 0,[ac2+0xA]; a REGION subscript (DERR 17 bound 100000)"),
            "w12": (12, 32, "FIRE.2@7016A483",
                    "XWLDA 0,[ac2+0xC]; a PLAYER subscript (DERR 17 bound 10)"),
            "w14": (14, 32, "FIRE.1@7016A3DA",
                    "XWSTA 0,[ac3+0xE] — WRITTEN uplevel, and read back at 7016A401"),
        }),
}


def check_frames():
    """The user ruling, enforced: width and a named witness, or it does not go
    in.  A missing witness is a HARD ERROR, not a warning — the whole point of
    the rule is that a slot cannot arrive by convenience."""
    bad = []
    for pname, p in PARENT_FRAMES.items():
        for fname, spec in p["locals"].items():
            slot, width, witness = spec[0], spec[1], spec[2]
            if width not in (8, 16, 32):
                bad.append("%s.%s: width %r is not 8/16/32" % (pname, fname, width))
            if not witness or "@" not in witness:
                bad.append("%s.%s: no named witness (expected NAME@PC)" % (pname, fname))
        for k, spec in p.get("args", {}).items():
            width, witness = spec[0], spec[1]
            if width not in (8, 16, 32):
                bad.append("%s arg %s: width %r is not 8/16/32" % (pname, k, width))
            if not witness or "@" not in witness:
                bad.append("%s arg %s: no named witness (expected NAME@PC)" % (pname, k))
            if not 1 <= k <= p["argc"]:
                bad.append("%s arg %s: outside argc %d" % (pname, k, p["argc"]))
        slots = [s[0] for s in p["locals"].values()]
        if len(slots) != len(set(slots)):
            bad.append("%s: two names for one slot" % pname)
    if bad:
        raise SystemExit("PARENT_FRAMES violates the P39 witness rule:\n  " + "\n  ".join(bad))


def c_type(width):
    return {16: "int16_t", 32: "int32_t"}[width]


def gen_h(digest):
    L = []
    w = L.append
    w("/* declarations.h — GENERATED by compiler/gen_declarations.py; do not edit.")
    w(" * Sources: docs/GAME_REFERENCE.md + docs/Project34/Census.md §3.")
    w(" * table sha256 (of declarations.json): %s" % digest)
    w(" * This is the only game/ file whose C and C++ views differ. */")
    w("#ifndef QUEST_DECLARATIONS_H")
    w("#define QUEST_DECLARATIONS_H")
    w('#include "quest_rt.h"')
    w("")
    w("/* ---- statics (word addresses in the data segment) ---- */")
    for name, (addr, width, cmt) in STATICS.items():
        w("/* %-10s 0x%08X: %s */" % (name, addr, cmt))
    w("")
    w("/* ---- record types: fields at their raw word displacement K from the base pointer ---- */")
    for tname, t in TABLES.items():
        w("/* %s: stride %d words, 1-based origin base+%d; %s */" % (tname, t["stride"], t["minK"] + t["stride"], t["comment"]))
        w("struct %s_rec {" % tname.lower())
        # lay the fields out by K relative to minK
        for fname, spec in sorted(t["fields"].items(), key=lambda kv: kv[1][0]):
            K, width = spec[0], spec[1]
            dims = "".join("[%d]" % d for d in spec[2]) if len(spec) > 2 else ""
            w("    %s %s%s;   /* K=%d%s */" % (c_type(width), fname, dims, K,
                                             ", row strides %s words" % list(spec[3]) if dims else ""))
        w("};")
    for pname, fields in DIRECT.items():
        w("struct %s_hdr {" % pname.lower())
        for fname, (K, width, cmt) in sorted(fields.items(), key=lambda kv: kv[1][0]):
            w("    %s %s;   /* K=%d: %s */" % (c_type(width), fname, K, cmt))
        w("};")
    w("")
    w("/* ---- parent frames: the static link's targets (P39) ----")
    w(" * A nested procedure's UPLINK(P) points at one of these.  Members are at")
    w(" * their WORD displacement off the parent's frame pointer; `__aK` is the")
    w(" * parent's K'th argument (by reference, at wfp-10-2K).")
    w(" *")
    w(" * This view NAMES the slots and type-checks uses; it does NOT reproduce")
    w(" * the frame's layout, and nothing should read offsetof() from it.  It")
    w(" * cannot: the arguments sit at NEGATIVE displacements (wfp-10-2K) and the")
    w(" * locals at positive ones, so no single struct puts both in address")
    w(" * order.  The translator does not read this struct — it reads the same")
    w(" * table from declarations.json, where the slot numbers are exact. */")
    for pname, p in PARENT_FRAMES.items():
        w("struct %s__frame {" % pname)
        for k in range(1, p["argc"] + 1):
            a = p.get("args", {}).get(k)
            ty = "%s *" % c_type(a[0]) if a else "void *"
            note = ("%s: %s" % (a[1], a[2])) if a else "width not witnessed yet"
            w("    %s__a%d;   /* the parent's argument %d, at wfp-%d — %s */"
              % (ty, k, k, 10 + 2 * k, note))
        prev = 0
        for fname, spec in sorted(p["locals"].items(), key=lambda kv: kv[1][0]):
            slot, width, witness, cmt = spec
            if slot > prev:
                w("    int16_t __pad%d[%d];" % (slot, slot - prev))
            w("    %s %s;   /* wp(link, %d) — %s: %s */" % (c_type(width), fname, slot, witness, cmt))
            prev = slot + (width // 16)
        w("};")
    w("")
    w("#ifdef __cplusplus")
    w("/* C++ view: real objects bound to the world image (native runtime, later). */")
    for name, (addr, width, cmt) in STATICS.items():
        if name in DIRECT:
            w("extern based_ptr<struct %s_hdr> %s;   /* 0x%08X */" % (name.lower(), name, addr))
        elif name.endswith("_PTR"):
            w("extern based_ptr<void> %s;   /* 0x%08X */" % (name, addr))
        else:
            w("extern %s %s;   /* 0x%08X */" % (c_type(width), name, addr))
    for tname, t in TABLES.items():
        w("extern ARRAY1(struct %s_rec, %d) %s;   /* via %s, origin +%d */" % (
            tname.lower(), t["bound"], tname, t["base"], t["minK"] + t["stride"]))
    w("#else")
    w("/* C view (pycparser / the translator): plain declarations; the numbers live in declarations.json. */")
    for name, (addr, width, cmt) in STATICS.items():
        if name in DIRECT:
            w("extern struct %s_hdr *%s;   /* 0x%08X */" % (name.lower(), name, addr))
        elif name.endswith("_PTR"):
            w("extern void *%s;   /* 0x%08X */" % (name, addr))
        else:
            w("extern %s %s;   /* 0x%08X */" % (c_type(width), name, addr))
    for tname, t in TABLES.items():
        w("extern struct %s_rec %s[%d];   /* via %s, 1-based, origin +%d */" % (
            tname.lower(), tname, t["bound"], t["base"], t["minK"] + t["stride"]))
    w("#endif")
    w("#endif")
    return "\n".join(x for x in L if x is not None) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(ROOT, "game"))
    args = ap.parse_args()
    check_frames()                      # the P39 witness rule, enforced
    table = dict(frames={p: dict(argc=v["argc"],
                                 args={str(k): dict(width=s[0], witness=s[1], comment=s[2])
                                       for k, s in v.get("args", {}).items()},
                                 locals={n: dict(slot=s[0], width=s[1], witness=s[2], comment=s[3])
                                         for n, s in v["locals"].items()})
                         for p, v in PARENT_FRAMES.items()},
                 statics={n: dict(addr=a, width=w, comment=c) for n, (a, w, c) in STATICS.items()},
                 direct={p: {n: dict(K=k, width=w, comment=c) for n, (k, w, c) in f.items()} for p, f in DIRECT.items()},
                 tables={n: dict(base=t["base"], stride=t["stride"], minK=t["minK"], origin=t["minK"] + t["stride"],
                                 bound=t["bound"], comment=t["comment"],
                                 fields={fn: dict(K=sp[0], width=sp[1], **({"dims": list(sp[2]), "strides": list(sp[3])} if len(sp) > 2 else {}))
                                         for fn, sp in t["fields"].items()})
                         for n, t in TABLES.items()})
    js = json.dumps(table, indent=1, sort_keys=True)
    digest = hashlib.sha256(js.encode()).hexdigest()[:16]
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "declarations.json"), "w") as f:
        f.write(js + "\n")
    with open(os.path.join(args.out, "declarations.h"), "w") as f:
        f.write(gen_h(digest))
    print("wrote declarations.h / declarations.json (table %s)" % digest)


if __name__ == "__main__":
    main()
