#!/bin/bash
# Populate a SCRATCH copy of a branch's Work/ + Disassembled/ for a task,
# WITHOUT touching the queue checkout's index.  Prints the scratch dir.
#
#   SRC=$(bin/task_source.sh <branch> <task-name>)
#   W=$SRC/Work
#
# Why (Sep 5 2026 finding): `git checkout origin/<branch> -- Work
# Disassembled` in the queue tree STAGES those paths, and the runner's
# `git add results/<name>` commit then carries the whole branch tree
# onto main (results: 037/038/039 all did this).  git archive into /tmp
# has no such side effect.  The relative provenance paths inside the IR
# headers (../../Disassembled/quest.dis) keep working because the
# Work/ + Disassembled/ layout is preserved.
set -eu
branch=$1; name=$2
cd "$(dirname "$0")/.."
git fetch --quiet origin "$branch"
SRC=/tmp/src-$name; rm -rf "$SRC"; mkdir -p "$SRC"
git archive "origin/$branch" Work Disassembled | tar -x -C "$SRC"
echo "$SRC"
