#!/bin/bash
# Task 081 -- meshsim WORKER9: what the runner sees of the w9 tarballs. Task 069 found no /tmp/w9-tracer.tgz although
# the owner copied both tarballs to /tmp on the box. Reads only; writes nothing outside results/.
cd "$(dirname "$0")/.."
echo "user: $(id)"; echo "host: $(hostname)"
echo "--- /tmp entries matching w9 (as the runner sees /tmp):"; ls -la /tmp 2>&1 | grep -i 'w9' || echo "(none)"
echo "--- /tmp owner and mode:"; ls -ld /tmp
echo "--- mount namespace of /tmp:"; findmnt /tmp 2>&1 | tail -2; readlink /proc/self/ns/mnt
echo "--- other likely places:"; ls -la /home/*/w9*.tgz /home/*/*/w9*.tgz /var/tmp/w9*.tgz 2>&1 | grep -v 'No such' || echo "(none)"
echo "--- free space:"; df -h /tmp /var/tmp 2>&1
for f in /tmp/w9-tracer.tgz /tmp/w9-delivery.tgz; do [ -r "$f" ] && sha256sum "$f" || echo "$f: not readable or absent"; done
exit 0
