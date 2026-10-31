#!/bin/bash
# Task 082 -- meshsim WORKER9: is /tmp/w9-tracer.tgz on the box with the expected SHA-256 (0d46b008...47f8df)?
# Gate for releasing 069b, so that 069b does not spend its attempts on a missing file. Reads only.
cd "$(dirname "$0")/.."
ls -la /tmp/w9-*.tgz 2>&1
f=/tmp/w9-tracer.tgz
if [ -r "$f" ]; then sha256sum "$f"; [ "$(sha256sum "$f" | cut -d' ' -f1)" = 0d46b00806b9529cb1d4e65b7354c5aab3fc54b1ba0db9bcc05ecf83b347f8df ] && echo MATCH || echo MISMATCH
else echo "$f: absent or unreadable"; fi
exit 0
