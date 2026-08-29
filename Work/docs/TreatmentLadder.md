# Routine treatment classification — RECONSTRUCTION (Aug 29 2026)

> The design doc of record, **M5FlatWorld.md**, was written in the
> Aug 23–24 2026 flat-graph session but never made it into Work/docs —
> it exists only in that session's downloaded outputs. THIS FILE is a
> reconstruction from the session transcript, faithful to the rulings
> but NOT the full doc (the original had more: the summarized-call
> model §4, tier tables, summary-difficulty classes, audit design).
> If the original file resurfaces, it REPLACES this one.

## The classification (user vocabulary, ruled in-session)

Every game routine gets exactly one treatment in the flat-graph
analysis world:

- **TERMINAL** — the routine never returns (ends process/session on
  all paths). Graph SINK: callers' blocks end at the call; no
  return-side exists, so fan-in is irrelevant and no summary of
  post-call state is needed. Spec = reads + writes + "no successors."
  Not a choice — a fact about the routine that, when true, trumps the
  ladder. Known/expected members: RETURN_MESSAGE (0310, proven);
  DIED (user recall: prints message, deletes player, ends game —
  VERIFY all paths terminate in the body scan; any conditional
  survive path makes it conditional-terminal: successors =
  {next, terminal}, returning path's effects summarized). Structural
  check is cheap: no WRTN on the path / no post-call code at call
  sites.
- **SUMMARIZE** — the body never enters the graph; every call site
  gets the routine's side-effect spec (a "summarized call" — the
  term that replaced "fat instruction"). Large fan-in; one summary,
  union over callers.
- **GRAPHIZE** — the body enters the graph and is analyzed in place,
  once, inside its single caller. Fan-in 1. Exact, zero aggregation
  loss; the routine is effectively a region of its caller living in
  its own area.
- **CLONE** — k copies, each graphized under its own caller, for
  fan-in k>1 routines we can't (or won't) summarize. Non-reentrancy
  makes the k analyses exact (activations never overlap in time);
  the area's final slot-split decision still unions the k clones'
  ranges (one physical area regardless). The escape hatch of last
  resort, always available — cost is pure bulk, never correctness.

**Treatment ladder (order of preference): terminal (if it applies) →
summarize → graphize → clone.** Nothing is unhandleable; the ladder
picks the cheapest sound treatment.

## Population sketch (from the session; re-derive before relying)

- Verb layer, fan-in 1 → GRAPHIZE: ~67 routines.
- Utility layer → SUMMARIZE: ~15–20 routines.
- Maybe-band, fan-in 2–5 → CLONE candidates: small; TERRAIN,
  GET_OBJECT_INDEX named. DIED (fan-in 7) looked like the stress
  case until the terminal insight removed it from the band.

## Status / where this fits

This is the M5 flat-graph ANALYSIS design (decompilation direction),
not part of the Gen-6 IR execution work. It is upstream input to the
eventual "get instructions out of the IR" / decompilation phases —
adjacent to roadmap item 3 but not required by items 1–4. Related
tree docs: PostM3b-FlatGraph.md, M5Notes.md (both predate this
classification and still use older vocabulary — read them with this
doc's terms).
