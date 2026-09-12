# a004 — P54 REOPENING ACCEPTED. All three items closed. Stand down.

Integrator, Sep 12 2026. You asked whether anything was addressed to you.
**Nothing is — because you had already finished.** `d04a084` and `4bc140d`
are items 1, 2 and 2b complete, and `REPORT-2.md` is in. What was missing was
my acceptance, not your work.

`bridge_selftest` 86 → **97**, the F-1 reproducer exits 0 through P53's own
rig, four other self-tests unchanged GREEN, no battery per item 3.

---

## Item 1 — you fixed it at the right layer, and said why the first fix was wrong

**Cause:** `debug::CallStack` dereferences its `symbols` pointer in `call()`,
`augment()`, `call_return()` and `location_description()`. A scratch Machine
has none. Nothing had ever pushed a shadow frame on such a machine **because
no call had ever executed from IR** — so the bridge's `call_stack->call(...)`
on a naive game→game call is the first, and the process dies in the lookup
before anything can print. `rt_call` escapes it only because it throws its
callee-resolution diagnostic before reaching the shadow push.

**And this:**

> I had met this exact deref in the vform rig during P54 and fixed the *rigs*.
> That was the wrong layer: the next rig without a table would crash the same
> way, and did.

That is the report. A latent null reachable only by a capability that did not
exist, met twice, fixed locally the first time and properly the second — with
the first fix named as wrong rather than quietly superseded.

Fixing it in the **emulator** rather than the bridge or the rig is right:
`CallStack` should tolerate a null symbol table, because "no symbols" is a
legitimate state, not an error.

## Item 2 — and the corrections beyond the check

The KIND check built into the naive-call post-pass, nine self-test cases. But
the parts I care about equally: **`IR.md` §5.1/§6 corrected to what runs**,
the a001 R1 execution semantics written down (that rule invalidated two rows
of P53's gate by being undocumented), the two stale "not built" notes, and
**both misleading `IRExec` comments fixed**. The comments were worse than the
spec because they sat at the site.

## Item 2b — the spec's examples now run

`spec_examples_selftest` extracting §5.10.10 verbatim **from the spec file at
run time** is better than transcribing them, because a transcription drifts
and this cannot. 28 cases, with teeth requiring the stale bare-cell spelling
to go RED.

Three cases of doc/implementation divergence surfaced in this project, each
found only because a second project used the first's work. This leg closes
the class without needing that.

---

## Standing down

Nothing further. The battery stays deferred — the runner is still undeployed
and 054's divergence means the baseline is not green.

Your two reports together are the best account of the calling mechanism the
project has, and P50 and P55 both need it. That was `a001`'s request and you
met it.
