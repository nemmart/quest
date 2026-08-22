#!/bin/bash
# Task 011 — B2 Stage 1 smoke (this session's self-generated run; 010 was the
# prior attempt). Build the emulator from main, fetch the 101-live B2 book from
# branch p14-phase-b2, load-parse it, assert 101 live. Emulator source is
# unchanged on the branch (only tools/build_address_book.py + the book differ),
# so we build main's tree and fetch only the book via `git show` — never check
# out the branch in the runner tree.
set -eu
cd "$(dirname "$0")/.."
echo "== toolchain =="
g++ --version | head -1; nproc
echo "== fetch B2 book from p14-phase-b2 =="
git fetch --quiet origin p14-phase-b2
BOOK=/tmp/quest.addrbook.b2-011
git show FETCH_HEAD:Work/c_src/quest.addrbook > "$BOOK"
head -2 "$BOOK"
echo "== emulator build =="
cd Work/c_src && make -j"$(nproc)" 2>&1 | tail -1 && ls -l emulator
echo "== book load-parse (101-live B2 book) =="
set +e
QUEST_ADDRESS_BOOK="$BOOK" ./emulator 2>/tmp/book_load_011.err
set -e
grep "AddressBook:" /tmp/book_load_011.err
LIVE=$(grep -oP 'AddressBook: .* — \K[0-9]+' /tmp/book_load_011.err | head -1)
[ "$LIVE" = "101" ] || { echo "FAIL: expected 101 live, got $LIVE"; exit 1; }
echo "== book line count cross-check =="
BOOK_LIVE=$(grep -c "^7" "$BOOK")
[ "$BOOK_LIVE" = "101" ] || { echo "FAIL: book has $BOOK_LIVE uncommented lines"; exit 1; }
rm -f "$BOOK" /tmp/book_load_011.err
echo "SMOKE OK"
