#!/usr/bin/env python3
"""
coverage.py — Block coverage tracking for translated functions.

Compares IR blocks against C++ block annotations to find untranslated code.

Usage:
  python3 coverage.py <function.ir> <function.cpp>

The C++ file should contain annotations like:
  // Covers: 701674B5, 701674BD, 701674C7, 701674C8

The IR file has blocks starting with hex addresses like:
  701674B5:

Reports:
  - Covered blocks
  - Uncovered blocks with their IR content
  - Blocks classified as trivial (just WBR/XJMP/WADC) vs computational
"""

import re
import sys


def parse_ir_blocks(ir_path):
    """Parse IR file, return dict of block_addr → [lines]."""
    blocks = {}
    current_addr = None
    current_lines = []

    with open(ir_path) as f:
        for line in f:
            line = line.rstrip()
            m = re.match(r'^([0-9A-Fa-f]+):$', line.strip())
            if m:
                if current_addr is not None:
                    blocks[current_addr] = current_lines
                current_addr = m.group(1).lower()
                current_lines = []
                continue
            if current_addr is not None and line.strip():
                current_lines.append(line.strip())

    if current_addr is not None:
        blocks[current_addr] = current_lines

    return blocks


def parse_cpp_coverage(cpp_path):
    """Parse C++ file for // Covers: annotations. Returns set of block addrs."""
    covered = set()

    with open(cpp_path) as f:
        for line in f:
            m = re.search(r'//\s*[Cc]overs?:\s*(.*)', line)
            if m:
                addrs = re.findall(r'([0-9A-Fa-f]{6,8})', m.group(1))
                for addr in addrs:
                    covered.add(addr.lower())

    return covered


def parse_coverage_file(cov_path):
    """Parse .coverage mapping file. Returns (covered set, section dict).

    Format:
      SECTION_NAME: addr addr addr ...
      # comment lines ignored
      # MISSING: notes about gaps
    """
    covered = set()
    sections = {}

    with open(cov_path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            m = re.match(r'(\w+):\s*(.*)', line)
            if m:
                name = m.group(1)
                addrs = re.findall(r'([0-9A-Fa-f]{6,8})', m.group(2))
                sections[name] = [a.lower() for a in addrs]
                for addr in addrs:
                    covered.add(addr.lower())

    return covered, sections


def classify_block(lines):
    """Classify a block as trivial or computational.

    Trivial: only contains WBR, XJMP, WADC, ENTER, RETURN,
             terminators (n/c/j/s/u), comments (/* */), or
             DERR/WSGTI/WSGT assert patterns.

    Computational: contains assignments, calls, block_copy, etc.
    """
    trivial_patterns = [
        r'^/\* WBR ',
        r'^/\* XJMP ',
        r'^/\* WADC ',
        r'^/\* WSGTI ',
        r'^/\* WSGT \d,\d \*/',
        r'^/\* DERR ',
        r'^/\* XNDO ',
        r'^/\* XNADI ',
        r'^/\* XNSBI ',
        r'^ENTER ',
        r'^RETURN',
        r'^[ncsju]\b',
        r'^/\*.*\*/$',           # any raw comment-only line
        r'^if \(',               # if-goto (control flow only)
    ]

    has_computation = False
    for line in lines:
        line = line.strip()
        if not line:
            continue

        is_trivial = False
        for pat in trivial_patterns:
            if re.match(pat, line):
                is_trivial = True
                break

        if not is_trivial:
            # Check for actual computation
            if ('=' in line and not line.startswith('//')
                or 'call ' in line
                or 'block_copy' in line
                or 'set_bit' in line
                or 'clear_bit' in line
                or 'store_byte' in line):
                has_computation = True

    return 'computational' if has_computation else 'trivial'


def main():
    if len(sys.argv) < 3:
        print("Usage: python3 coverage.py <function.ir> <function.cpp|.coverage>",
              file=sys.stderr)
        sys.exit(1)

    ir_path = sys.argv[1]
    cov_path = sys.argv[2]

    blocks = parse_ir_blocks(ir_path)

    if cov_path.endswith('.coverage'):
        covered, sections = parse_coverage_file(cov_path)
    else:
        covered = parse_cpp_coverage(cov_path)
        sections = {}

    # Classify all blocks
    total = len(blocks)
    covered_count = 0
    uncovered_trivial = []
    uncovered_computational = []

    for addr, lines in sorted(blocks.items()):
        if addr in covered:
            covered_count += 1
        else:
            kind = classify_block(lines)
            if kind == 'trivial':
                uncovered_trivial.append((addr, lines))
            else:
                uncovered_computational.append((addr, lines))

    # Report
    print(f"Block Coverage: {covered_count}/{total} "
          f"({100*covered_count/total:.0f}%)")
    print(f"  Covered:                {covered_count}")
    print(f"  Uncovered (trivial):    {len(uncovered_trivial)}")
    print(f"  Uncovered (computation):{len(uncovered_computational)}")
    print()

    if uncovered_computational:
        print("═══ UNCOVERED BLOCKS WITH COMPUTATION ═══")
        print()
        for addr, lines in uncovered_computational:
            print(f"  {addr}:")
            for line in lines:
                # Truncate long lines
                if len(line) > 90:
                    line = line[:87] + "..."
                print(f"    {line}")
            print()

    if uncovered_trivial and '-v' in sys.argv:
        print("═══ UNCOVERED TRIVIAL BLOCKS ═══")
        print()
        for addr, lines in uncovered_trivial:
            print(f"  {addr}: {'; '.join(l for l in lines if l.strip())}")

    if sections:
        print()
        print("═══ SECTION SUMMARY ═══")
        print()
        for name, addrs in sections.items():
            verified = sum(1 for a in addrs if a in blocks)
            print(f"  {name:25s} {verified:3d} blocks claimed")


if __name__ == "__main__":
    main()
