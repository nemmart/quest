#!/usr/bin/env python3
"""
dataflow.py — CFG construction and dataflow analysis for MV-32 basic blocks.

Reads the same basic block format as ir_convert.py.
Provides:
  - CFG construction with predecessor/successor edges
  - Forward dataflow framework (iterate to fixed point)
  - ac3 frame pointer tracking analysis
  - (Future) live range analysis for frame slots
"""

import re
import sys
from collections import defaultdict


# ── Block representation ─────────────────────────────────────────

class Block:
    def __init__(self, addr, instructions, terminator):
        self.addr = addr
        self.instructions = instructions
        self.terminator = terminator
        self.successors = []    # block addresses
        self.predecessors = []  # block addresses
        self.is_entry = False
        self.is_exit = False    # WRTN, I.STOP, no successors

    def __repr__(self):
        return f"Block({self.addr}, succ={self.successors}, pred={self.predecessors})"


# ── Parser ───────────────────────────────────────────────────────

def parse_blocks(text):
    """Parse basic block format into Block objects."""
    blocks = {}
    current_addr = None
    current_insts = []
    first_addr = None

    for line in text.split('\n'):
        line = line.strip()
        if not line:
            continue

        # Block header
        m = re.match(r'^([0-9a-fA-F]+):$', line)
        if m:
            if current_addr is not None:
                blocks[current_addr] = Block(current_addr, current_insts, None)
            current_addr = m.group(1).lower()
            if first_addr is None:
                first_addr = current_addr
            current_insts = []
            continue

        # Terminators: n, c, s, j, u
        if re.match(r'^[ncsju]\b', line):
            if current_addr is not None:
                blocks[current_addr] = Block(current_addr, current_insts, line)
                current_addr = None
                current_insts = []
            continue

        # Instruction
        if current_addr is not None:
            current_insts.append(line)

    if current_addr is not None:
        blocks[current_addr] = Block(current_addr, current_insts, None)

    # Mark entry block
    if first_addr and first_addr in blocks:
        blocks[first_addr].is_entry = True

    return blocks


# ── CFG construction ─────────────────────────────────────────────

def build_cfg(blocks):
    """Build successor/predecessor edges from terminators."""

    for addr, block in blocks.items():
        term = block.terminator
        if not term:
            block.is_exit = True
            continue

        # Extract successor addresses from terminator
        # Formats:
        #   n addr [addr...]
        #   c target n addr [addr...]
        #   s NNN n addr [addr...]
        #   j target n addr [addr...]
        #   u

        if term.startswith('u'):
            block.is_exit = True
            continue

        # Find the "n" part and extract addresses after it
        m = re.search(r'\bn\s+(.*)', term)
        if m:
            addrs_str = m.group(1).strip()
            if addrs_str:
                succs = [a.lower().replace('0x', '') for a in addrs_str.split()]
                block.successors = succs
            else:
                # "n" with no addresses = dead end
                block.is_exit = True
        else:
            block.is_exit = True

    # Build predecessor lists
    for addr, block in blocks.items():
        for succ_addr in block.successors:
            if succ_addr in blocks:
                blocks[succ_addr].predecessors.append(addr)

    return blocks


# ── Forward dataflow framework ───────────────────────────────────

def forward_dataflow(blocks, entry_addr, init_state, transfer_fn, meet_fn):
    """
    Generic forward dataflow analysis.

    Args:
        blocks: dict of addr → Block
        entry_addr: starting block address
        init_state: initial state for entry block
        transfer_fn: (block, in_state) → out_state
        meet_fn: (list_of_states) → merged_state

    Returns:
        dict of addr → (in_state, out_state)
    """
    # Initialize
    states = {}
    for addr in blocks:
        states[addr] = (None, None)  # (in_state, out_state)

    # Entry block gets init_state
    states[entry_addr] = (init_state, transfer_fn(blocks[entry_addr], init_state))

    # Worklist algorithm
    worklist = list(blocks[entry_addr].successors)
    visited = {entry_addr}

    max_iterations = len(blocks) * 3  # safety bound
    iterations = 0

    while worklist and iterations < max_iterations:
        iterations += 1
        addr = worklist.pop(0)

        if addr not in blocks:
            continue

        block = blocks[addr]

        # Compute in_state from predecessors
        pred_states = []
        for pred_addr in block.predecessors:
            if pred_addr in states:
                _, out = states[pred_addr]
                if out is not None:
                    pred_states.append(out)

        if not pred_states:
            continue

        in_state = meet_fn(pred_states) if len(pred_states) > 1 else pred_states[0]
        out_state = transfer_fn(block, in_state)

        old_in, old_out = states[addr]

        # Check if state changed
        if old_out != out_state:
            states[addr] = (in_state, out_state)
            # Add successors to worklist
            for succ in block.successors:
                if succ in blocks:
                    worklist.append(succ)

        visited.add(addr)

    return states


# ── ac3 frame pointer analysis ───────────────────────────────────

# Import ac3_kills_frame from ir_convert
sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')
from ir_convert import ac3_kills_frame


def ac3_transfer(block, in_state):
    """Transfer function for ac3 frame tracking.
    State is a bool: True = ac3 is frame pointer, False = dirty."""
    state = in_state
    for inst in block.instructions:
        kills, restores = ac3_kills_frame(inst)
        if restores:
            state = True
        elif kills:
            state = False
    return state


def ac3_meet(states):
    """Meet function: conservative — if ANY predecessor has dirty ac3,
    entry state is dirty."""
    return all(states)


def analyze_ac3(blocks):
    """Run ac3 frame pointer analysis on CFG.
    Returns dict of addr → (entry_ac3_is_frame, exit_ac3_is_frame)."""

    # Find entry block
    entry_addr = None
    for addr, block in blocks.items():
        if block.is_entry:
            entry_addr = addr
            break

    if entry_addr is None:
        # Use first block
        entry_addr = min(blocks.keys())

    # ac3 is always frame at function entry (after WSAVS)
    states = forward_dataflow(
        blocks, entry_addr,
        init_state=True,
        transfer_fn=ac3_transfer,
        meet_fn=ac3_meet
    )

    return {addr: (in_s, out_s) for addr, (in_s, out_s) in states.items()
            if in_s is not None}


# ── Reporting ────────────────────────────────────────────────────

def report_cfg(blocks):
    """Print CFG summary."""
    print("CFG Summary:")
    print(f"  {len(blocks)} blocks")

    entries = [a for a, b in blocks.items() if b.is_entry]
    exits = [a for a, b in blocks.items() if b.is_exit]
    print(f"  Entry: {', '.join(entries)}")
    print(f"  Exits: {', '.join(exits)}")

    # Find loops (blocks that are their own successors, or back edges)
    for addr, block in sorted(blocks.items()):
        for succ in block.successors:
            if succ <= addr:  # back edge (assuming addresses increase)
                print(f"  Back edge: {addr} → {succ} (likely loop)")

    print()


def report_ac3(blocks, ac3_states):
    """Print ac3 analysis results, highlighting blocks where ac3 is dirty on entry."""
    dirty_entries = []
    for addr in sorted(ac3_states.keys()):
        in_state, out_state = ac3_states[addr]
        if not in_state:
            dirty_entries.append(addr)

    if dirty_entries:
        print(f"ac3 Analysis: {len(dirty_entries)} blocks with dirty ac3 on entry:")
        for addr in dirty_entries:
            in_s, out_s = ac3_states[addr]
            print(f"  {addr}: entry={in_s} exit={out_s}")
    else:
        print("ac3 Analysis: ac3 is always frame pointer at block entry (clean)")
    print()


# ── Main ─────────────────────────────────────────────────────────

def main():
    if len(sys.argv) > 1:
        with open(sys.argv[1]) as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    blocks = parse_blocks(text)
    build_cfg(blocks)

    report_cfg(blocks)

    ac3_states = analyze_ac3(blocks)
    report_ac3(blocks, ac3_states)


if __name__ == "__main__":
    main()
