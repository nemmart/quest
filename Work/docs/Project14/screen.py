#!/usr/bin/env python3
# screen.py <session.log> [tail_bytes] — reconstruct the D215 screen from the
# raw byte stream (Dasher cursor addressing). Project 14 Phase B recon tool.
# 0x10 c r = write cursor (col,row); 0x0C = erase page + home; CR col=0; LF row+1;
# 0x17/0x1A/0x18/0x19 = cursor up/down/right/left; mode codes ignored.
import sys
data = open(sys.argv[1], 'rb').read()
if len(sys.argv) > 2:
    data = data[-int(sys.argv[2]):]
ROWS, COLS = 24, 80
grid = [[' '] * COLS for _ in range(ROWS)]
r = c = 0
i = 0
while i < len(data):
    b = data[i]
    if b == 0x10 and i + 2 < len(data):
        c, r = data[i+1], data[i+2]
        c %= COLS; r %= ROWS
        i += 3; continue
    if b == 0x0C:
        grid = [[' '] * COLS for _ in range(ROWS)]; r = c = 0
    elif b == 0x0D: c = 0
    elif b == 0x0A: r = (r + 1) % ROWS; c = 0   # D215 NEW LINE = CR+LF
    elif b == 0x17: r = (r - 1) % ROWS
    elif b == 0x1A: r = (r + 1) % ROWS
    elif b == 0x18: c = (c + 1) % COLS
    elif b == 0x19: c = (c - 1) % COLS
    elif b in (0x0E, 0x0F, 0x14, 0x15, 0x1C, 0x1D, 0x00, 0x08, 0x0B): pass
    elif 0x20 <= b < 0x7F:
        grid[r][c] = chr(b); c = (c + 1) % COLS
    i += 1
for row in grid:
    print(''.join(row).rstrip())
