#!/usr/bin/env python3
"""
string_annotate.py — Annotate IR string references with actual content.

Reads memory from quest.dis hex dumps, then replaces:
    ac3=byte_addr(0x70167435)  // string
with:
    ac3=byte_addr(0x70167435)  // "INVENTORY"

Usage: python3 string_annotate.py <quest.dis> < input.ir > output.ir
   or: python3 ir_convert.py blocks | python3 string_annotate.py <quest.dis>
"""

import re
import sys


def load_memory(dis_path):
    """Load word-addressed memory from quest.dis hex dump lines."""
    memory = {}
    with open(dis_path) as f:
        for line in f:
            line = line.rstrip()
            if not line or line[0] not in '0123456789abcdef':
                continue
            if '    ' not in line or '[' not in line:
                continue
            parts = line.split()
            if len(parts) < 9:
                continue
            try:
                addr = int(parts[0], 16)
                for i in range(1, 9):
                    if len(parts[i]) == 4 and all(
                        c in '0123456789abcdefABCDEF' for c in parts[i]
                    ):
                        word = int(parts[i], 16)
                        memory[addr + i - 1] = word
            except (ValueError, IndexError):
                pass
    return memory


def read_string(memory, word_addr, max_len=40):
    """Read printable string from word address (high byte first).
    Skips leading 0x0A/0x0B (message newlines). Once printable text
    is found, 0x00/0x0A/0x0B terminate. DG attribute codes skipped."""
    result = []
    wa = word_addr
    bo = 0  # 0=high byte, 1=low byte
    found_printable = False
    for _ in range(max_len * 2):
        if wa not in memory:
            break
        word = memory[wa]
        ch = ((word >> 8) & 0xFF) if bo == 0 else (word & 0xFF)
        if ch == 0:
            break
        # 0x0A/0x0B: skip if leading, terminate if after content
        if ch in (0x0A, 0x0B):
            if found_printable:
                break
            # Skip leading control codes
        elif 0x20 <= ch < 0x7F:
            result.append(chr(ch))
            found_printable = True
        # DG terminal attributes (0x0E, 0x0F, 0x14, 0x15, 0x1C, 0x1D) — skip
        bo += 1
        if bo > 1:
            bo = 0
            wa += 1
    return ''.join(result)


# Pattern: byte_addr(0xNNNNNNNN)  // string
RE_STRING_REF = re.compile(
    r'(byte_addr\(0x([0-9A-Fa-f]+)\)\s*//\s*)string'
)


def annotate_line(line, memory):
    """Replace // string with // "actual content" where resolvable."""
    def replacer(m):
        prefix = m.group(1)
        addr = int(m.group(2), 16)
        s = read_string(memory, addr)
        if s:
            # Truncate long strings
            if len(s) > 35:
                s = s[:32] + "..."
            return f'{prefix}"{s}"'
        return m.group(0)  # Can't resolve — leave as-is

    return RE_STRING_REF.sub(replacer, line)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 string_annotate.py <quest.dis> [input.ir]",
              file=sys.stderr)
        sys.exit(1)

    memory = load_memory(sys.argv[1])

    if len(sys.argv) >= 3:
        with open(sys.argv[2]) as f:
            text = f.read()
    else:
        text = sys.stdin.read()

    for line in text.split('\n'):
        print(annotate_line(line, memory))


if __name__ == "__main__":
    main()
