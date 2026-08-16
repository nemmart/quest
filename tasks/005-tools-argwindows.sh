#!/bin/bash
# Task 004 — build the Java tools, regenerate argmap/callsites, diff vs committed
set -eu
cd "$(dirname "$0")/.."
ROOT=$(pwd)
cd Tools
javac -nowarn *.java */*.java
jar -cf tools.jar *.class */*.class
java -cp tools.jar ArgWindows "$ROOT/QUEST" QUEST ../Disassembled/quest.addrs ../Disassembled/quest.targets ../Work/c_src/quest.addrbook ../Disassembled/quest.dis /tmp/argmap.new /tmp/callsites.new
sort /tmp/argmap.new > /tmp/a1; sort ../Disassembled/quest.argmap > /tmp/a2
diff /tmp/a1 /tmp/a2 && echo "argmap: identical"
sort /tmp/callsites.new > /tmp/c1; sort ../Disassembled/quest.callsites > /tmp/c2
diff /tmp/c1 /tmp/c2 && echo "callsites: identical"
echo "TOOLS OK"
