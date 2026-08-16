#!/usr/bin/env python3
"""
slot_naming.py — Stage 2 slot naming with cross-block reaching definitions.

1. Parse IR into blocks
2. First pass: identify all def sites (slot, width, block)
3. Build CFG, run reaching definitions (forward dataflow)
4. Second pass: rename uses where exactly one def reaches

If multiple defs reach a use (merge point), the slot stays raw.
Cross-block uses with a single reaching def get renamed.
"""

import re
import sys
from collections import OrderedDict


# ── Regex patterns ───────────────────────────────────────────────

# Def: s16[0xNN]=... or s32[0xNN]=...
RE_DEF = re.compile(r'^(s(?:16|32))\[(0x[0-9A-Fa-f]+)\]\s*=')

# Use: s16[0xNN] or s32[0xNN] anywhere in text
RE_SLOT = re.compile(r'(s(?:16|32))\[(0x[0-9A-Fa-f]+)\]')

# Address-of: &s[0xNN] (no width, in call args)
RE_ADDR_OF = re.compile(r'&s\[(0x[0-9A-Fa-f]+)\]')

# Block header
RE_BLOCK = re.compile(r'^([0-9A-Fa-f]+):$')

# Terminator lines
RE_TERM = re.compile(r'^[ncsju]\b')


# ── Definition tracking ──────────────────────────────────────────

class Def:
    """A single definition of a frame slot."""
    __slots__ = ('name', 'width', 'slot', 'block', 'index')

    def __init__(self, name, width, slot, block, index):
        self.name = name
        self.width = width      # "16" or "32"
        self.slot = slot        # "0x1E" etc.
        self.block = block      # block address string
        self.index = index      # instruction index within block

    def __repr__(self):
        return f"{self.name}(s{self.width}[{self.slot}]@{self.block})"

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other):
        return isinstance(other, Def) and self.name == other.name


class SlotAnalysis:
    def __init__(self):
        self.all_defs = []          # list of all Def objects
        self.next_id = 0
        # Per-block info
        self.block_defs = {}        # block_addr → [Def, ...]
        self.block_kills = {}       # block_addr → set of (slot, width) killed
        self.block_insts = {}       # block_addr → [line, ...]
        self.block_order = []       # ordered block addresses
        self.block_succs = {}       # block_addr → [successor addrs]
        self.block_preds = {}       # block_addr → [predecessor addrs]
        # Reaching defs results
        self.reaching_in = {}       # block_addr → set of Def
        self.reaching_out = {}      # block_addr → set of Def
        # Entry block
        self.entry_block = None
        self.func_name = None

    def new_name(self, width):
        """Generate unique temp name."""
        n = self.next_id
        if n < 26:
            suffix = chr(ord('a') + n)
        elif n < 26 * 27:
            suffix = chr(ord('a') + n // 26 - 1) + chr(ord('a') + n % 26)
        else:
            suffix = f"_{n}"
        self.next_id += 1
        return f"t{width}_{suffix}"

    # ── Phase 1: Parse and identify defs ─────────────────────────

    def parse_ir(self, text):
        """Parse IR text into blocks, identify all defs."""
        lines = text.split('\n')
        current_block = None
        current_insts = []
        inst_index = 0

        for line in lines:
            stripped = line.strip()

            # Function name comment
            if stripped.startswith('# '):
                self.func_name = stripped[2:]
                continue

            # Block header
            m = RE_BLOCK.match(stripped)
            if m:
                if current_block is not None:
                    self._finish_block(current_block, current_insts)
                current_block = m.group(1).lower()
                current_insts = []
                inst_index = 0
                if self.entry_block is None:
                    self.entry_block = current_block
                self.block_order.append(current_block)
                continue

            # Terminator
            if RE_TERM.match(stripped):
                if current_block is not None:
                    # Parse successors
                    succs = self._parse_successors(stripped)
                    self.block_succs[current_block] = succs
                    current_insts.append(stripped)
                    self._finish_block(current_block, current_insts)
                    current_block = None
                    current_insts = []
                continue

            if current_block is not None:
                current_insts.append(stripped)

        if current_block is not None:
            self._finish_block(current_block, current_insts)

        # Build predecessor map
        for addr in self.block_order:
            self.block_preds[addr] = []
        for addr, succs in self.block_succs.items():
            for s in succs:
                if s in self.block_preds:
                    self.block_preds[s].append(addr)

    def _finish_block(self, addr, insts):
        """Process a completed block: find defs and kills."""
        self.block_insts[addr] = list(insts)
        defs = []
        kills = set()

        for i, line in enumerate(insts):
            stripped = line.strip()
            if not stripped or stripped.startswith('//') or stripped.startswith('/*'):
                continue

            # Check for def
            m = RE_DEF.match(stripped)
            if m:
                width = m.group(1)[1:]  # "16" or "32"
                slot = m.group(2).upper()
                name = self.new_name(width)
                d = Def(name, width, slot, addr, i)
                defs.append(d)
                self.all_defs.append(d)
                # Kill: this slot (both widths, since they share storage)
                kills.add((slot, "16"))
                kills.add((slot, "32"))

            # Check for call with &s[] args — invalidates those slots
            if 'call ' in stripped:
                addr_slots = RE_ADDR_OF.findall(stripped)
                for slot in addr_slots:
                    slot = slot.upper()
                    kills.add((slot, "16"))
                    kills.add((slot, "32"))

        self.block_defs[addr] = defs
        self.block_kills[addr] = kills
        if addr not in self.block_succs:
            self.block_succs[addr] = []

    def _parse_successors(self, term_line):
        """Extract successor addresses from terminator line."""
        succs = []
        # Find 'n' and collect addresses after it
        m = re.search(r'\bn\s+(.*)', term_line)
        if m:
            for addr in m.group(1).split():
                addr = addr.lower().replace('0x', '')
                if re.match(r'^[0-9a-f]+$', addr):
                    succs.append(addr)
        return succs

    # ── Phase 2: Reaching definitions analysis ───────────────────

    def compute_reaching_defs(self):
        """Forward dataflow: compute reaching definitions at each block."""
        # Initialize
        for addr in self.block_order:
            self.reaching_in[addr] = set()
            self.reaching_out[addr] = set()

        # Transfer function: out = (in - killed) ∪ gen
        def transfer(addr, in_set):
            kills = self.block_kills.get(addr, set())
            # Remove any def whose (slot, width) is killed
            survived = {d for d in in_set
                       if (d.slot, d.width) not in kills}
            # Add this block's defs
            gen = set(self.block_defs.get(addr, []))
            return survived | gen

        # Worklist algorithm
        worklist = list(self.block_order)
        changed = True
        iterations = 0
        max_iter = len(self.block_order) * 5

        while worklist and iterations < max_iter:
            iterations += 1
            next_worklist = []

            for addr in worklist:
                # Meet: union of all predecessors' out sets
                preds = self.block_preds.get(addr, [])
                if preds:
                    new_in = set()
                    for p in preds:
                        new_in |= self.reaching_out.get(p, set())
                elif addr == self.entry_block:
                    new_in = set()  # Entry has no reaching defs
                else:
                    new_in = self.reaching_in[addr]  # Keep current

                new_out = transfer(addr, new_in)

                if new_in != self.reaching_in[addr] or new_out != self.reaching_out[addr]:
                    self.reaching_in[addr] = new_in
                    self.reaching_out[addr] = new_out
                    # Add successors to worklist
                    for s in self.block_succs.get(addr, []):
                        if s not in next_worklist and s in self.block_preds:
                            next_worklist.append(s)

            worklist = next_worklist

    # ── Phase 3: Rename ──────────────────────────────────────────

    def rename_ir(self, text):
        """Second pass: rename slot references using reaching def info."""
        lines = text.split('\n')
        output = []
        current_block = None
        live_defs = {}  # (slot, width) → Def or None (ambiguous)
        decl_insert = -1

        for line in lines:
            stripped = line.strip()

            # Function name
            if stripped.startswith('# '):
                output.append(line)
                continue

            # Block header
            m = RE_BLOCK.match(stripped)
            if m:
                current_block = m.group(1).lower()
                # Initialize live defs from reaching_in
                live_defs = {}
                for d in self.reaching_in.get(current_block, set()):
                    key = (d.slot, d.width)
                    if key in live_defs:
                        live_defs[key] = None  # Ambiguous: multiple defs reach
                    else:
                        live_defs[key] = d
                output.append(line)
                continue

            # ENTER — mark for declaration insertion
            if 'ENTER' in stripped:
                output.append(line)
                decl_insert = len(output)
                continue

            # Terminator — pass through
            if RE_TERM.match(stripped):
                output.append(line)
                continue

            # Empty/comment
            if not stripped or stripped.startswith('//'):
                output.append(line)
                continue

            # Raw MV-32
            if stripped.startswith('/*'):
                output.append(line)
                continue

            if current_block is None:
                output.append(line)
                continue

            # Process the line
            result = self._rename_line(stripped, current_block, live_defs)
            output.append(f"    {result}")

        # Insert declarations
        if decl_insert >= 0 and self.all_defs:
            decls = ["    // --- Local variables ---"]
            for d in self.all_defs:
                decls.append(f"    // {d.name}: s{d.width}[{d.slot}]")
            decls.append("")
            output = output[:decl_insert] + decls + output[decl_insert:]

        return '\n'.join(output)

    def _rename_line(self, stripped, block_addr, live_defs):
        """Rename slot refs in a single line. Updates live_defs in place."""

        # Check for def: s16[0xNN]=...
        m_def = RE_DEF.match(stripped)
        if m_def:
            width = m_def.group(1)[1:]
            slot = m_def.group(2).upper()

            # First rename uses on the RHS
            rhs = stripped[m_def.end():]
            rhs = self._rename_uses(rhs, live_defs)

            # Find the Def object for this def site
            new_def = None
            for d in self.block_defs.get(block_addr, []):
                if d.slot == slot and d.width == width:
                    # Could be multiple defs in same block for same slot
                    # Use the first one we haven't consumed yet
                    key = (slot, width)
                    if key not in live_defs or live_defs[key] != d:
                        new_def = d
                        break
            if new_def is None:
                # Fallback: search all defs in this block
                for d in self.block_defs.get(block_addr, []):
                    if d.slot == slot and d.width == width:
                        new_def = d

            if new_def:
                # Kill old defs of this slot
                for w in ("16", "32"):
                    k = (slot, w)
                    if k in live_defs:
                        del live_defs[k]
                live_defs[(slot, width)] = new_def
                return f"{new_def.name}={rhs}"

            return stripped

        # Call with &s[] — rename known refs, then invalidate
        if 'call ' in stripped:
            result = self._rename_uses(stripped, live_defs)
            # Replace &s[0xNN] with &name where known
            def addr_replacer(m):
                slot = m.group(1).upper()
                for w in ("16", "32"):
                    key = (slot, w)
                    if key in live_defs and live_defs[key] is not None:
                        return f"&{live_defs[key].name}"
                return m.group(0)
            result = RE_ADDR_OF.sub(addr_replacer, result)

            # Invalidate slots passed by reference
            for slot in RE_ADDR_OF.findall(stripped):
                slot = slot.upper()
                for w in ("16", "32"):
                    k = (slot, w)
                    if k in live_defs:
                        del live_defs[k]
            return result

        # General: rename uses
        return self._rename_uses(stripped, live_defs)

    def _rename_uses(self, text, live_defs):
        """Replace s16[N]/s32[N] with temp name if exactly one def reaches."""

        def replacer(m):
            width = m.group(1)[1:]
            slot = m.group(2).upper()
            key = (slot, width)
            if key in live_defs:
                d = live_defs[key]
                if d is not None:  # Single def reaches
                    return d.name
            return m.group(0)  # Ambiguous or unknown — leave raw

        return RE_SLOT.sub(replacer, text)


# ── Main ─────────────────────────────────────────────────────────

def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    analysis = SlotAnalysis()
    analysis.parse_ir(text)
    analysis.compute_reaching_defs()
    result = analysis.rename_ir(text)
    print(result)


if __name__ == "__main__":
    main()
