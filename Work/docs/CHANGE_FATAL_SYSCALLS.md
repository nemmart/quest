# CHANGE: Four AOS/VS system calls for the ?FATAL path (FATAL_SYSCALLS)

Companion to `CHANGE_FLOAT_SHADOW.md`. Together these five changes take
`?FATAL` from dying partway through to producing a complete PL/1 call
traceback — the report the game itself would print, which nothing in
this project had ever seen.

- **Date:** Aug 2026
- **Files:** `os/OSContext.{hpp,cpp}`, `os/OSContextSystem.{hpp,cpp}`,
  `os/OSContextShared.{hpp,cpp}`, `os/OSContextTask.{hpp,cpp}`,
  `os/OSProcess.hpp`
- **Risk:** very low — all four are reached only on the terminal
  `?FATAL` path, which the clone never executes, so none can contribute
  to a lockstep divergence.

## Check whether you already have it

```
grep -c "RNGPR\|ERMSG\|SCLOSE\|DFRSCH" os/OSContext.hpp     # 4 if applied
grep -c rescheduling_disabled os/OSProcess.hpp              # 1 if applied
```

## Why these, and why now

`?FATAL` is the PL/1 fatal-condition reporter. It runs only after a
signal reaches no handler, so nothing had ever executed it. Each fix
exposed the next wall:

| Wall | Where | Fix |
|---|---|---|
| `Floating point underflow` | `C?TRIM` | `CHANGE_FLOAT_SHADOW.md` |
| `Unimplemented system call 0251` | `?FATAL`+0x1E2 | `?RNGPR` |
| `Unimplemented system call 0161` | `?FATAL`+0x635 | `?SCLOSE` |
| `Unimplemented system call 0311` | 0x7017F346 | `?ERMSG` |
| `Unimplemented system call 0550` | `SWAT.REX` | `?DFRSCH` |

## 1. `?RNGPR` (0251) — returns the .PR filename for a ring

`OSContextSystem`. Doc: AC0 = PID / byte pointer to a process name /
-1 for self; AC1 = -1 when AC0 is a byte pointer, anything else
otherwise; AC2 = word pointer to the packet. Output: length of the
string, in packet offset `?RNGLB`.

Packet as built by `?FATAL` at 0x7017F3E9–F3F9:

```
+0 (wide)  byte pointer to the result buffer
+2 (word)  ring number
+3 (word)  ?RNGLB — buffer size in, string length out
           (?FATAL reads it back at 0x7017F400)
```

`?FATAL` masks a return address with `0x7000` and shifts right 12 to get
the ring, then asks which `.PR` is loaded there — turning an address
into a module name. Everything Quest executes is ring 7, so other rings
return `ERRNL`; the caller's error return at 0x7017F3FC already handles
that by omitting the name. Returns `:QUEST.PR`.

## 2. `?SCLOSE` (0161) — closes a file opened for shared access

`OSContextShared`. AC0/AC2 reserved (0); AC1 DG **bit 0**
(`0x80000000`) = release the shared pages, DG bits 2-31 = the channel
number from `?SOPEN`.

Note the bit numbering: DG counts bits MSB-first, so `WIORI 1,0x80000000`
at 0x7017F666 is setting *bit 0*, the release flag — not a channel bit.
`?FATAL` walks its chain of open shared files and closes each with pages
released. Page release is a no-op here: the emulator drops mapped pages
at termination anyway.

**Known oddity:** in the observed run the channel decoded to −2
(`0x3FFFFFFE` after masking), so the call takes its `ERFNO` error return
rather than closing anything. Either that chain entry is a terminator,
or the bits-2-31 reading is off by one. Harmless today — the caller
loops correctly on the error — but unresolved.

## 3. `?ERMSG` (0311) — reads the error message file

`OSContextSystem`. AC0 = error code; AC1 DG bits 16-23 = buffer byte
length, DG bits 24-31 = channel of the message file (`0377` selects the
system default ERMES); AC2 = byte pointer to the buffer. Output: AC0 =
byte length, AC1 = channel used.

Quest passes `AC1 = 0x0000FFFF` at 0x7017F343. DG bits 16-23 are C bits
15-8, so that decodes as **255-byte buffer, channel 0377** — the default
ERMES file. A 16-bit length reading gives 65535 and is wrong; only the
manual explains the split.

We have no ERMES file. Codes in the emulator's own table render from it;
for anything else the **documented** behaviour is a NORMAL return
carrying `UNKNOWN ERROR CODE n`, which is what we emit. Observed:
code `0x2004` → `OS - invalid channel number`.

## 4. `?DFRSCH` (0550) — disable rescheduling, report prior state

`OSContextTask`. No input. Output AC0 = `?DSCH` (0x8000) if rescheduling
was **already** disabled. No error codes defined.

Distinct from `?ERSCH` (0526) / `?DRSCH` (0527), which `MT?ERSCH` /
`MT?DRSCH` use — `?DFRSCH` also reports the prior state so a callee can
restore it.

Reached only from `SWAT.REX` (0x7017E4BF, 0x7017E4E8) — SWAT being DG's
symbolic debugger — which tests the result against `?DSCH` exactly:

```
7017e4bf SYSCALL 0550
7017e4c3 WXORI 0,0x8000     ; was it already disabled?
7017e4c7 WBR   -> store 0 at 0x700001B2
7017e4c8 NLDAI 7,0          ; otherwise store 7
```

`OSProcess` gains `bool rescheduling_disabled = false`. The emulator
runs tasks as real threads and does **not** gate scheduling on this — it
is tracked only so the prior state is reported truthfully. Scheduling
starts enabled, so the first call returns 0 and `SWAT.REX` stores 7.

## Error codes: use `aos_symbol`, do not invent

`os/AOSVSSymbols.cpp` already holds every AOS/VS error mnemonic. Two
placeholder constants were created before this was noticed, then
deleted. Use:

```cpp
return static_cast<int32_t>(aos_symbol("ERRNL"));   // 0x7E1C ring not loaded
return static_cast<int32_t>(aos_symbol("ERIRB"));   // 0x0010 insufficient room
return static_cast<int32_t>(aos_symbol("ERFNO"));   // 0x0002 channel not open
return static_cast<int32_t>(aos_symbol("ERTXT"));   // 0x006E text too long
aos_symbol("?DSCH")                                 // 0x8000
```

## Validation

Lockstep regression: zero divergences, clean shutdown, only the three
known-benign exceptions.

`QUEST_FAIL_OPEN=USER_DATA_FILE` + `L` -> `P` now runs `?FATAL` to
completion:

```
Call Traceback:

from fp=16000011010,  pc=16005767100
from fp=16000010760,  pc=16005761757
from fp=16000010740,  pc=16005761720
from fp=16000010626,  pc=16005761406
from fp=16000010530,  pc=16005665247
from fp=16000010350,  pc=16005705001
from fp=16000010230,  pc=16005342740
from fp=          0,  pc=16005765124

OS - invalid channel number
```

Spot-checked as genuine: `16005767100` octal = 0x7017F440, inside
`?FATAL`; `16005342740` = 0x7016EC60, inside `LIST_PLAYERS` — exactly
where the failure was injected.

## Consequence for the ?FATAL terminal-registration design

`HeapSignalPlan.md` / `ERROR_LIFT_SCOPE.md` propose leaving `?FATAL`
emulated permanently: on a fatal condition the master runs it and prints
this traceback while the clone rendezvouses, dissolves the pair, and
exits. **That design is now viable** — before these fixes the master
died partway through and printed nothing.

Still to build for it: a terminal registration that marks the clone-side
stub *without* setting `translated_bits` (`?FATAL` never returns, so the
master must not arm run-to-return on it), a rendezvous before dissolving
so an asymmetric arrival is still reported as a divergence, and a gate
release so the master can run free.
