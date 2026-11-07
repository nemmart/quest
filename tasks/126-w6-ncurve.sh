#!/bin/bash
# Task 126-w6-ncurve -- meshsim sim/ WORKER6 (branch sim-w6 2a1ccb5). TIMING TASK: keep the box quiet while it runs.
# W6's addition for D15 (sim/bench/mesh.cpp): throughput against N, N independent meshes one per thread, N = 1..12 on the
# P-cores (CPUs 0,2,4,6,8,10, then the siblings 1,3,..,11), (a) and (b), both mixes, rate 0.35; the same with 16 meshes per
# thread (past L2); the footprint per mesh (bytes the sizing allocates); ready marks per delivery, none, and on transitions,
# at rates 0.01, 0.1, 0.35. perf stat per run (L1 and LLC loads for the bytes touched). 3 repeats. Expected ~6 min.
# From an embedded kit (sim/src/base and sim/bench at 2a1ccb5). Writes only results/126-w6-ncurve/. Scratch in /tmp, removed first
# and at the end. Safe to re-run.
set -u
cd "$(dirname "$0")/.."
REPO=$PWD; RES=$REPO/results/126-w6-ncurve; mkdir -p "$RES"; rm -f "$RES/raw.txt"
W=/tmp/w6-126-w6-ncurve; rm -rf "$W"; mkdir -p "$W"
sed -n '/^__KIT_BEGIN__$/,/^__KIT_END__$/p' "$0" | sed '1d;$d' | base64 -d > "$W/kit.tgz" || exit 2
GOT=$(sha256sum "$W/kit.tgz" | cut -d' ' -f1); echo "kit sha256 $GOT"
[ "$GOT" = "83c5b197319924d8e71fdba7da25b7a5fbc9e7cf71f35b878fe9a356092716af" ] || { echo "kit sha256 mismatch (want 83c5b197319924d8e71fdba7da25b7a5fbc9e7cf71f35b878fe9a356092716af)"; exit 2; }
mkdir -p "$W/kit" && tar -xzf "$W/kit.tgz" -C "$W/kit" || exit 2
K=$W/kit/sim/bench
FAILS=0; FAIL=0
sec() { echo; echo "==== $*  [$(date +%T)]"; }
verdict() { echo "VERDICT $1: $2"; [ "$2" = PASS ] || FAIL=1; }
START=$(date +%s)
{
sec "machine"; uname -r; nproc; lscpu | grep -E 'Model name|^CPU\(s\)'; uptime
# Shared by W6's task bodies (sourced). Needs W (scratch), RES (results dir), K (kit's sim/bench dir).
# run TAG CPUSET cmd...: one run under taskset and perf stat; appends "TAG <bench line> perf: ..." to $RES/raw.txt.
EV=${EV:-cycles,instructions,cache-misses,L1-dcache-load-misses}
run() {
  local tag=$1 cpus=$2; shift 2
  taskset -c "$cpus" perf stat -x, -e $EV -o "$W/perf.csv" -- "$@" > "$W/out.txt" 2> "$W/err.txt"
  local rc=$?
  local line; line=$(grep -E '^(mesh|fp128) ' "$W/out.txt" | tail -1)
  if [ $rc -ne 0 ] || [ -z "$line" ]; then
    echo "RUNFAIL $tag rc=$rc: $*"; head -5 "$W/err.txt"; FAILS=$((FAILS + 1)); return 1
  fi
  local pc; pc=$(awk -F, '
    $3 ~ /cycles/ && $3 !~ /ref/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; c[k] = $1 }
    $3 ~ /instructions/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; i[k] = $1 }
    $3 ~ /cache-misses/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; m[k] = $1 }
    $3 ~ /L1-dcache-load-misses/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; l[k] = $1 }
    $3 ~ /LLC-loads/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; ll[k] = $1 }
    $3 ~ /LLC-load-misses/ { split($3, a, "/"); k = (a[1] == "cpu_atom") ? "atom" : "core"; lm[k] = $1 }
    END { printf "core_cycles=%s core_instr=%s core_cmiss=%s core_l1miss=%s atom_cycles=%s atom_instr=%s atom_cmiss=%s",
          c["core"], i["core"], m["core"], l["core"], c["atom"], i["atom"], m["atom"]
          if (("core" in ll) || ("core" in lm)) printf " core_llc_loads=%s core_llc_misses=%s", ll["core"], lm["core"] }' "$W/perf.csv")
  echo "$tag $line perf: $pc" | tee -a "$RES/raw.txt"
}
# field NAME < lines: the value of NAME=...
field() { grep -o "$1=[^ ]*" | cut -d= -f2; }
# Task body: W6's addition (D15): throughput against N, footprint per mesh, and the ready marks' cost by load.
cd "$K" || exit 2
EV=cycles,instructions,cache-misses,L1-dcache-load-misses,LLC-loads,LLC-load-misses
sec "build (gcc -O2)"
make -j8 > "$RES/build.log" 2>&1 || { tail -20 "$RES/build.log"; exit 2; }
B=build/gcc-O2/mesh
L="--dl e1 --width fx --rate 0.35"     # task 124's best layout and width
ORDER=(0 2 4 6 8 10 1 3 5 7 9 11)      # one hyperthread per P-core up to 6, then the siblings
cpus() { local n=$1 s=""; for ((i = 0; i < n; i++)); do s="$s${s:+,}${ORDER[$i]}"; done; echo "$s"; }

sec "1. footprint per mesh (bytes the sizing allocates), (a) and (b)"
for lw in a b; do run "footprint" 0 $B --lw $lw $L --cycles 1000 --warmup 100 --footprint --cpus 0; done

sec "2. throughput against N: N independent meshes, one per thread, N = 1..12 (CPUs ${ORDER[*]}), 3 repeats"
for rep in 1 2 3; do for lw in a b; do for mix in 128 mixed; do for n in 1 2 3 4 5 6 7 8 9 10 11 12; do
  run "ncurve-1" 0-11 $B --lw $lw --mix $mix $L --meshes $n --threads $n --cpus $(cpus $n) --cycles 300000 --warmup 30000
done; done; done; done

sec "3. throughput against N with 16 meshes per thread (~2.3 MB each, past L2: shared L3 and memory), 128-bit, 3 repeats"
for rep in 1 2 3; do for lw in a b; do for n in 1 2 4 6 8 10 12; do
  run "ncurve-16" 0-11 $B --lw $lw --mix 128 $L --meshes $((16 * n)) --threads $n --cpus $(cpus $n) --cycles 20000 --warmup 2000
done; done; done

sec "4. ready marks by load: per delivery (bm), none/polling (poll), on empty -> non-empty (tr); one mesh, CPU 0, 3 repeats"
for rep in 1 2 3; do for lw in a b; do for rate in 0.01 0.1 0.35; do for rd in bm poll tr; do
  cyc=300000; [ $rate != 0.35 ] && cyc=1000000
  run "ready" 0 $B --lw $lw --dl e1 --width fx --mix 128 --rate $rate --ready $rd --cycles $cyc --warmup $((cyc / 10)) --cpus 0
done; done; done; done

sec "5. digests"
for mix in 128 mixed; do for n in 1 6 12; do
  d=$(grep '^ncurve-1 ' "$RES/raw.txt" | grep "mix=$mix " | grep " meshes=$n " | field digest | sort -u | wc -l)
  [ "$d" = 1 ] && verdict "ncurve-1 mix=$mix meshes=$n: (a), (b), repeats agree" PASS || verdict "ncurve-1 mix=$mix meshes=$n ($d distinct)" FAIL
done; done
for rate in 0.010 0.100 0.350; do
  d=$(grep '^ready ' "$RES/raw.txt" | grep "rate=$rate " | field digest | sort -u | wc -l)
  [ "$d" = 1 ] && verdict "ready marks rate=$rate: every mode agrees" PASS || verdict "ready marks rate=$rate ($d distinct)" FAIL
done
[ "$FAILS" = 0 ] && verdict "all runs completed" PASS || verdict "all runs completed ($FAILS failed)" FAIL
sec "done in $(($(date +%s) - START)) s"
} 2>&1 | tee "$RES/run.log"
grep '^VERDICT' "$RES/run.log" > "$RES/VERDICTS.txt"
rm -rf "$W"
grep -q ': FAIL$' "$RES/VERDICTS.txt" && exit 1
exit 0
__KIT_BEGIN__
H4sIAAAAAAAAA+29+WIbN7I3ev49fAqEka2mRVIktdihTOU4tjzjL94+24nPuYrCaZJNqSNu6Sa1
xHbe6b7CfbJbvyoAjV4oybbszJlRz8Rio7EWgEJVoZY4HK/HUX+958fB+n98madBz92tLf5LT/Zv
/nez0by7/R9q6wv1J/Us4rkfKfU1mvpnfGJ3/v3RaNrvHi78aFDvz2bX1QYmdXtzc8n8Nxtb2xuZ
+d9oUpJqXFcHLnr+zef/23DSHy0GgSrnVsBRuVSyn+/34/lgEAx3M0mjsOcmTYLTXaeUVDr05/6I
q5v44yCe+f1A0bpT75z3d6X5URT4gy61749UbzodqcNuOOlGi4nqqKE/ioMdpdbXVX86oSmbzGvh
JJyH/iiMg0FboWw4OVThXE2CkyBSPBJ/HsSlD6XSyTQcqF5wKPV5FWpO0fPo4d/3Hv7ofWMaqlKH
TSYVTuKQhuAreilXdriA06N5tAh2qG6uOpgMiip26tU51HQxv7BaGSjVyxBI+ksDnC+iic26o9JN
d4fTqMuA5tx50BEUPjD8UnNQKlFKjR41PwrUQwO1aVRVVCH1lv47pV73fgv6czUMR0GsvNrbUbVW
O438Wader1dKwdk8oK6VH5Zp+OjUHdXt0oSMumOuz4vDP4LunAab+th3P1ZVcSb8g1ycmMlk8gyj
IJAM2dK0Og4nwaC7pKVwMjc5Z9M4POuOgzEXkdpse06R0re0CcJJoF4/edb9208PXj3yTo98yjGp
qOt5fuElEQ6V1+32FuFoTjMZnM0I/jSAcbtt1kRVNSoV9fjBmwdPvfKtWE2H6tYfC9U7pzWfXrxV
JV008+BNKpWSBRTmMT1RGIus42SQZclRxkBl0eoFmZ5pfPyQrjo1zerYwLKwkb5p5FjdWdKQru5Y
OpJpK7Va1OzCtnTepSMydc3clkxDyZJTM1M3poxenCaQq4x5kvrd5TrLd75osSr/wjGkiiwdSbpi
34xHVj83Xbj6Hfhd3It06aXdyDRCtduufEjw0HQWREBAhMZPlT8ZqEEwCuZBVQGrnwMrjVPHhoCQ
TywaIhWyi7gqR4XqH/nRHd4C+b6bvSv91RNKSDOzqtX3aqLaqqnzYaa/mS3bfG0geaTQUKfRud1+
GajMkgXg9N5MljMKOmXbbU7unlCfeDY+dmRmHkc0Njo952GfNlI8vy/pu57vjIwy3ecC06HGqhVd
0E3LQmyyGI1m82jHwV5Fs34b0z6ijlmQqm86tEOuCZYWlLJqEkSwDGBmKqWinUIgLt+/mRPVIFV3
Dbs41R7izmoFceDmJ5pAfSioZ//gY2vaP1hWV26L8BKbTIkEm55257crajINzvrBbH5tfb6mlq40
pvyGKYaY3WsTRkUfP6TraOgTR3QFeLob5OqjLn3CqL9MZxgyZl/bD+mNnVo/mb0/c+uTtATU2Qpp
hJ9WpTt/y3pp6cfr7+0nVn21Xl++Za9rENfR0hVnIrN6v8ycfF4jH7OmvtaIrqexT5ujr7kSP6Ft
4eGvNnbCZ3+1yCf1LJX/HV1fGxfL/xrNzc3tjPyvtb3RuJH/fY2HyNc3R4GR+4TTiTqaTo/bxHmd
u4m8iJ33XjA/DYJJSqYGZi0RhIWxErkfcXYl3cpw5B/iA+0LJeI+5QV+/wgE/tvN1VgnxpBaxCqc
xyx8mvnRPESrlbp66C8Oj+ZthxlE2SU8I5r1omA2ItJ8oMKJysi3K1XOLEwe7XP9VzP+VQVCH9yK
wz1zgTRLw+Wo/mE0HRuBGVpOy8xG4eSYMp2G8yOVlp8RygkCFr89848DZCdAvH314CUN9/mUeBY9
5AT6MXV5EBgZzyjs9QGrYDQErNXfpyOC4GIyICA/eO1P1Hw6bUvHXDBhGqJgGEQRdWs+5ayrsUAE
I4q5S4CF8vCtFk7mQQSEFwwqGmb10rezyD8c+2o6AfeTFe/m5K4iwRXpaJswKDhSzPg5zw6mPSNP
3QFqdYrYbCnx6M5SUag0yLzlKq+mtuqPAj+S0fF6xJSAyewfBf3juFhK+lfv0i/3pPF/FEz867z5
kecy/L/RuJvB/827d5s3+P9rPNn7H14B6Zuf+DxeH4/9Sf1oN3shlLn9oaQonBzuFmACV2THBFVw
NouMWOr474vDQAuYunOvVVH376tWY8dc3dzRmLsXHho+lKVDabEOmjBiHU2zGakX0HRHi8Z3O7q9
7/Xftjp+Gk6CH/B1J18af6iwJ6XXdG01SK7W9csd+XupSEyq6qiGev9eFQrG6JfQ2iIW43wooauq
lMwtgRaXWbio1Vvxatu5ARj6dJIMIIwlsFQlMSXkY5hoWFQUE8AVOldOwpipV3Siqp49ePRz9+8/
/W3v5YO/7Wlsijw0j9PJ6Lyt+C6EKgQ6PQ6iSTBSsX8eE24XuSOoZhpjHMxRaUMPLi+8EwyOE5dn
ORGtSxVG5rbsGuuv3kb/a58C/H+NlL88l+D/rdZGK4v/Nzfu3uD/r/HQTnoZRITdg8mcbwaY4pWr
8wGTdcqfAyEOw8NFJMT/PBwHQGbj9Vd7Dx4926uPB8TtPhMJvaZacXsMnNA/J3pLjabTmWEEElSu
cTjjbaJQCZH8EUTTgCrb3qzho6G9U5zI0DAGO6qlnv1g8zBpbRBYGm8xbY62lRRZzJR3BNQ/8w8J
VY4DP16ADP6z2bgFEvPti1c/7r1qfUdjeUzjAHr1owhozeN7iBDdnvs9ouyJh4inGOm51juIw7nl
jnw1P58ROS9NC0OgSWwDnx4N7FioUcKM0GCY+XFMFHh0Qj2j83TRJyQJfE5DqCcI0txYgE2iXhKy
p1wppkv9gKqJb5AfANxEnU6jgdSVzIMwOgTAI+oy9f5wCoaFJ286OQkmIZYGwZsGR3X4SadW0cEp
MQBBhRt8Q4N9ABRy/80u8ytmyMyj0fQCGnxmz2kxTRc4v31C4eGcWLnwDPM8l7MXDdBk79B8Hgee
Hx3GUDRIvgnAJsHZHPUKeGnemNOrCghkOEM6N3eIHZRi04hZyyoYIEqiRqN4XpWcqJDxH48/y9lc
qgdD3FFODyZ5XczDEY3yAtWYTDrPU6HKzMcSRDtLTtadUqk/wlKT1fGuNFv0RmG/zSczp5mauS9F
pFZbPnU990ZxQZDY3uzO7+x6SR9l2d0xN4gmT0Vv/gqd8x+44T+lZZz7tsfSCJ/9Sd+kN/z7doVo
LeH4d5Icty2727kws+2vDMbTqyy5x5HmmfKYReEJ4cV2UcEuUTEE03kwplVIqOs+FjtGp97sakjb
/aGJVA02+hREc6HICDhviPrtOCQpoVZbsC3Ybowd6femJ9Dw6PvEuaoRZYd4MTWNtpyZSrPdCmbT
UpZtNaBFmZ7UN6nZ9Oyu/T7ZwHyVa2f4TTK1ySbv2oL6a9fTNLue/KS/ho7Hc3pElKxWXlG7uCvm
Du7Xapx0UP/zjacpSjx23XAu/eFDBh4yfPueWxXJl9wyKi7EpfKTT2hLPdiVKu8INntw+zZSgdTc
URr+oAui3ELM3osLagKRzzitnQCeSH5N5XcdUDsAeSPcSLsNsU9qWhkX7AqgiLPh5mkvvvGE5p5G
p340uP9glzFwBSg4qXVtjbMnCQk1LwCXQSfwS+4QQ3fcWlUu1DoHNAZnrBD4DIIzZmv4fGcOKjXi
UN9NVHI9kVUSHrgrQE/hkl7pnf8X9Y3mCcmuth9nM4jP4UfzWEomI53TLIZ8brtMdlJwSdBBvggP
KY8Fdae7O9ku0joGu5e0VNiOxpo3LN1f9aT4v144H/uza2cAL+H/tputHP+33dq84f++xgM+gWe9
TdxBD5wV0dFgv0JL+BFyCeYgzaFwXUf2GG9gjILzKfEyExDOhv2hrJ7gTSKd9T0NI0rmlECqD+m1
y6S34YS6uATqougEjMhkSidlfEzMkOEZH4KLrIGLJAwyCHB14PCEVby+fvO0qo4CfxBENYimPpeC
T2SZn0+ya0Kb4ZyhtOU4Vi6p3QUQHA1AzIpzJkTTBcFvPu2C5Iu9iZCtkksEZKiXgPaaFjamivgw
vvOoW/Ekz6SXELCai0uUPFHZcuEqHujI8Sx3X7958YqYbM9WUhY0oo9B5+yT0bEGImdNvnCD+MI/
nHQcEUimv0kqjwb3cV4lRWzw0CBpLKIz+HrHC3Pd2Q/V7q7aPlDvO1j/NseHpEpu79Mqvd1Rf+Zr
5csrIu3nF9Ae+ar1/HuZJuhfolBuq+0NIt1uq2YKJPridTRlfhc7DBtbdq8/nNOG/QeEAf+oIkFT
FiLNFWnChNjrunu0J5vX9J2FCazCmR0CSFr+uqsntlux1AS/JmMz6x/2ClyChpZ8NStVndF3PfxT
Aq7y/jSfvAZfGkh7Agxnwbjy/AkacXeNS55pXuOMpfTuTJvxrK2dsjz+dOlQ8KT6mXz6kJ9LdHq7
YojvuaPz35//QSv8rFLJTuiQVgXLjwS56kmtKj/uBxMYwdTVUI3pXOMLVtq+k0PcL9NsysZcjbUY
CJ/PqfRJGIfzYFBfwsOox7vJXnCxtff49m01LFi6VwY3RuE5c9/YoT/3KfsO8RenlRRMOW/xSthR
Z/R/bLYzuZgZeleCbBq0vCv9iUMuf+ERYT3ZMdj1JIZFmYWizYKSRaSbmE1nfToP5gV9NkwAd+L6
R0IVr3XysDUdIgAnQ8sjsX4xkhWkbi599L2R1MOXR+m+FgiUrJTInAWmxk8AAKZncqoxgasfztAt
6uDZcIg+FvYrXa2cd4ynzDKmcrR2D3DVaJFaU5Cam72CXKl1eyFfqLGThgphj7fUmvL7/SCOxcbL
kTHzJIJs8zWqEDHwiT9aBLTUpiNCHUfUt0o9JQHjEdilskSAxts0tXaR3s2sXtvt9Jx8LpeaprJ0
5UvoK2mU8Abmhs8h022pxI4a57pDFJjy2cnTZ3NqYcrhnjv8XYGDDN1SU1rkgNMbAgdtFRExbveM
5KGSEj04wCsUWGZvqF14dBOskWPZ6VO5fMO3/2s8af5/0T8O5l+b/9/cytn/E/9/Y//9VR7w/zzr
cgE7CEYhU5WyFHBXqfwZ0YEDZqrlGhSYh+/raqfhYH6kgskcV6JIdiUHdWY98PFcSUaw+3SggKO3
4muIDhz5gpw4kp0oVh/an4ReiFNpq7fb5qo2VvPTqaZi/UM/BIaC7mjAJ5qP25Bj/gyVURAWwUD1
ztEulQ3mc9yzygUnnT519YBHyIlTKg0BuwaAvVGFWCTQkAlHgx1rcD7zuW1dF/ILEnf1Xk+PgmAE
8EilMXK93VglCH2KYEM9UCDQo9QcsCrs6XQxGqjjIJihZSb5gzO/L4wfG9HNj6YQ6cRT0VqNYjWY
TlYNl/BPKDORWbiyzGQxiY02AEGlymfeRsuRhS8XpGhSlguCtLQldlLyhWWik0tb/gx5CgMhL09h
/oFnv+OaUOqVKwSDfGdusMeqHLQb6HfzY6QzXAe+8A+HlzCyfZXcWLmcBu2Hbpr7yMtz0nIKvtFn
jLHDWh56h0eiHy1LFmritPcjYlnrGbpGMJWXvVbL2dGbrjkXbWJNPwiDLna/l+dYNHgsOyl1rK1h
qQiAciyN7o5Mu0N9uQxOf3bumW5X1akRwcm8LmdxHMosqZgB59mFd/GFlvTfri69XKSKW4uKXGkJ
TruVvtKSkpdDKCwGjZ5qc2j4MR88fLDouj1b0KsYIfQwmlJPaOJ7fv+4Xjh81jO/8P4+ob9Zood5
cFap7l+ycYtZEymx44rygvGs8JbNLrOk8hSWKOisgCvXla9wk7e/P5lKjoMD1e36c5qf3mIedLse
LeHRAIdROAGupI3CUEx2S26h5VFR7tp6YZDXLZdvcTak3gIpPHEZD5OGr4N9cqA095MZLPXvxu2k
6H85pr+6/n+jtZnT/9/evqH/v8az1FVTmtDzo8Mc7Tct9AWVJfGyHqWKbYS4XcI5nrv/YAtFJw7h
fWCdNP00HBNzwkqBgnSW2v7AhQPfTEbBbBox5T+Ysv5m3uwtkZoPwwga7kSEaIoT+jCELifzIZRj
giiik5ORnLoVt28N2opQmPSXMaQ2BfCJtsQ158y+Y7XN6cSvYgTGOQdqP8lWzyP0Z0lVoBHsu+7R
Yt73Vn+ZrLLdKhVLfR2OFvGRl//g9wgQHtui/kvitJvn6k8B/v/a+v+t7a2c/dfm1o3971d5jHUk
y2aMlSXbRkLsQkTWMewmIfohJDKN1BHlGwGJepOpEgN4WKTuqIw5wKO9h09eP3nx/LV6tFkxshCh
ChPULY8/OoVq/XTSVowCY4NZgU/bQKdtdZ/wU0xM6W4ZPICgNGFRNNpkfD5J4TfWSdccDx0dg6pz
aAyC3uJQRDl6eB448Ed7P/z0t0pbg4Tr13rwA7ZABu8JFpW4SqSJdGgUB3X1ZEIHDHhrrY1e+KTq
QuF4HkI7YzqewVhLefEUCvi4eK2JOSpPC7wOAOSsmL+YLGJY4BJprlVuAtzP+HyBq7VlDI09pc/D
0fQ0Vl4Z5He5wry0jC4r0BqHMdWsvNl0JhrxzNVU6bRc9EaBNoYWlUf39qHiirw+RXyUpwi+DYeU
WdkJKQnboRK7Qea6jh/xJBpXjN8GNBGXZdX3qN9C3DcslVI8z+cSIhmeCRI5qscsz42q2oQuNHFS
uA0sdsZonAvKRuFa2232+mf71e0+fvJ0r0v8Ubf79Mlz/evnB90Hr/72ututJJUsWfrX/4jHwsGU
WMyv9fySEvBoGOlZvn1b5SQ+3zAYtM/Ez20Sz1Wmpcx7WFtitkEoqrJMxLeMPXjaXrx8Q6ulqiqp
WdxxmvxgtFIalesnzVLnP4S313/8X27/18qd/xvNG/3Pr/IQDnpFsw4zscdPHr/IXu3Eo+m84GLn
LX/UZj3w5oF8Fe16Y4ZPXLCSvd2patMznFKDoD/yI33Pwcccn2b6JkI9xiXM0tsXe1gFwTFU13Bo
pW9iKvmrmKtettDfkzCaL6hd9oSRuX7hGx0rmKam6LimLrG7DBzTMg5qBsf3XG5igvGVrm3+Je5s
sJ4+48bGvPNCuvp1jWT/xLuaVJufflEDQF52TQOTftkjqUsbXjnulc2thbOd6EWucIgSLLzHqZqh
aDB8xq0O19Bl06aZm4490M0qk338FQ8QA2j4IShfT8w4oZxa+aQbnxkkHB9x3yODW37ZY1dCSEOQ
Ea8ZiX+qjRBKrVIbYRpVMy+ugVS23Mfdk/AK5vFdeoEkUPjY6yM9IWGN1hRRpawgjMnxGqCp6QvG
Xym+awHe9VLAKtD/LbptctY51/GRV022yd+c+QnTU/ObOzW/FU3Nkon4rXgi0qPujhfz3MidKx8C
Qc4Q1rW5EyABR9IHoPtKbZchEVYy2IvONK/AUk4v5l3cXqTAmeLbDBgd2NVqRQt5bU1v7ARmmZ1u
rl7MmL/EvRhnT98l5XPrafy0q7QUis/nTtf9T3WJZuf4ildoBsdd0/1ZNTkRGtVkbVz56kwHbRgN
2iJogYaOlubE/7LXav9rnhT/x4q+XSj6XisXeAn/t93czvl/3Gpt3fB/X+OhnfczZv0lTboIfsUH
42qc6Po1W/eY+ePlISJJkGqQCEIgGPaDGNzNzD8fTX04E5yyyxSi2eaRfxKAe2OhnLxB5YP9wEy0
NJFQ0lQYH2kx7VmmIgyXRXqQmjrqglqlUBcV1uswmp7G2mGiHIkwY6KSbH3IvV7GV+Ir0K3tXErR
Gq4T2Weh8Y+TYzEfo3ZhmkHD+sboz2uyZVMQ0HCkX8Z9yzwxzzIADc6Y23MuJ6mdAXQgxzTmmEiJ
HaoZivhMQKNdXQOQOtVIwBkTNI4I6UKdEdXS/FWYxYTDGOHCoyDu+5MJhsvvDTOI18IqELt6TsOX
VaHn3mtpCh02QJqbZ981uNYEoOTFmBNOVXM75cmnouX07PpGQMO3rLHxmsnM3afpRX6a9DnD3RoT
6Bzb+wns8CIGbBe0fWjy7cna7VLDlGYdsNj99xFMc4FaY5a6zfDLLc0LQHXNFIJ1lLb+bafqT3Qf
Mxyctillit2x7eU1tuMuFLYIhvmZaN9ewfb0GhUmE4OVPDcu/QNtEgXs0nRG8NIUO80KU+vi0yjj
UqTOHdcfCXBXgW/VGUymH1KntVC6qlYlMbWLOMjw3EAFV2XDLZgltMnFplHSSddQp9Bkk9vPGVGK
tRjE8Uk1HvGuIvdYWztNMyI5rl0bWi5n1s2wnYoSPlVUILHx9MsVTAJT/axUcrMlaoOuTe7amp6Q
HHMZ5k2T2Ymhq55ZzConGnDOQtZMHlcBnjltAOSwBGkboCJ/MCbqmYyJzZDDysWNDZMDMXUAOm0V
Le0UqGq1HKgw7Z7YMVdo5DyfFTutoWMFbBYv7dCU75ZiaC61ndabv9ApzIV1X9k6O99Cwt9asBVV
bdeNuwSwedLzlFMNFaDmGVQD7L9CldSxcbuqNvJFi1772clavX2ZRW8bC+PL1vqHL8byZ3qVZ/yX
e3xydsod57BLcfmaKGVYXKwemztuNNbPnDj/blqz/zpPiv/XtOx1t3GZ/u/du9n4r82t5g3//1Ue
h90Tb58SYCA+0jZziRNX4cYSa3HDLe6JFwqTrub+cSDBGwytrCMjcODTtvpHitOASso/xGX00fSU
2NbJeequGdwqYaC4KvQ29WkxU3P2I3RK/G3guD8k7pS9JsLCPdaxHf7hEu2iAPMP7enfuCmqCMPs
MK5A/RkPtHX1empMF9N+WifKH7AIYoyeSfSE6CRgoxZ27upEuzBBDBNgJcyv5nyvhYldHnA3rx9l
XLBbf5OEurc3dy7OyCBWrpdKtZ67aQKDmy1deIkrM+2a4MvUr7mNaYfrTsod54VpEN1lprXwywYX
k+PJ8Slu2smzgPROxyRCIs7gTcdzRogeVOytRSoQ7BJG9DiE6pVwrqLLo8kQrT2U6qRhO4kouRVr
YsQsyFszQ45k1mXZbaSq3JFWbI2VCw5h4P9eMOkffang7//xCfHfkXvzJv7713iS+TfRZ66/jUvO
/83GRjN7/rearZvz/2s83yq7AmBgvxor/j32I5yQs5EfTthtbZXz0WfEzJE7gIzxTIXOrtK3hOOQ
XWUfrRxdVYf9vqq9aLVF4r5Or/S2/o4Q01F1OCOu5UNSCbVQ0zra/PijeKor2LA1qRp1tn/UmVBX
TqCcOyL2kD94LPuXdwl3FBN9U0nqF/3M5AEN4GiGS+CjqvrpB/pTMUGnTvwo9CdzAsUgPCQ+MlbB
7wuEtGWteYjcuVq4sWclCtgVwZwoTto9JqLGfQQUlFqfH/7RVi5NzpC2U1TV/g3+7wLS/rkfQzs/
GPeCASgjqsAZG8F7Uio9fNh9/uDZnvq+A3iVXrx8wy3S64tW6eF//3cXUFSqTZ/X1jhB4EUJ/AOJ
L549e/Gc89SI4uj019ZaDVUbTqa1xABA3qP5PFS1t3A5WHsbnM0jH3/ZcKD2pF7HqPBD1Z49e0T/
vFS1mVBGJQS8UtIRJzyWDs2l3/qpNxOmS7+yfrr+nQrZZRLT8VYAiS6tEdMm1tGj56Juzp82kk8b
6U+y0Gwpd/WlM8pSkoxNVaNF+cjqtBO8Yp9oUyKAOkRARnDHBPoWJMVAgGm/nxBFkk6qRUEfyv0d
QDqtmKwk53Qczmtsw1GbTZnIKT1++uBvr9GbFU+mtEK/0E/+F46lDrEAZNSUh9YC/Svrh2iIHwxA
ZLUmn2pSvvTDg9d7uuwp5SAyeKBkzoW7vMMx10o/vPjh/+hsKNF2s9xClg59qJj3aaX09wc/73Uf
Pn3w/G9SKj4KxGhijL1ROzELVbV21wfByTpEHUTy1F/+/cXz/5E4BA4ikT2PHSg7xIdkhVsECpJf
jIdKpaRUu/Sf/7XiPXvw4x5BqUbgZa3+2oC2NaIKnCsCQIc21FVybVwllyyn0n+ueOGQ+pSAoFK9
uLCelI7sYukWASMF0rbKwZz6ND6mWmhDUnP4sfJfFTRPqwDLhNcONdlXK/dVbUpfpUqu7fNq0NCn
mvCDFkdBqZVfpQilcGQ84iwwjVeaFt6Fpf+sr3OROs1xbgFLlhItirY7DM5X+s8+L+N6HcJY0Aq1
WjyN5h12jVirjRENpfNfhBBrHCOjg1+HxOfM+NdkMQ6isC/faPR/DBNsvp7C+7SP/xNScsKpYOU6
mWzpU8F+xCUiLWPqdzRWtWioO12qv957+OL5owev/qddqhnu0OwdOPHUR1yNh7F6pz5YTe+fv5ow
uXm+ypOsMrM9rr+NS/i/zbt3s/o/zVbjxv7/qzzffrPeCydAKkclhxfQx6SXkMlE/0YLXCtdTCVD
MScG2cYSH8iziGgQwpnPV009q8MQ8Y1Y6AYEJKS08ljBfRyesYH+6VRFPoetU82qajEpvGljxKKG
bAU73JwlxP1DIgrjxI4zHk1PFQc9DTiw1URHjXp5Pj9CtCFWf5qeqwVRWWB1ptFYWJ0jjqnK1e0k
KkeW/+FWE0LfRGZFJhtbiqWnMPAsp1RZysKZEDHHWi1z5ccxHRnQu4aaU21R+qGz8q7Z/l5gTSfT
h9LjB0+edhqlqcTJCfpHVOkUfMzKnTJEUz1/4HxBbvnC5ZrIMeAIUu8UDWiGk3VVwNfZb9S+82vD
gzur6r3qL6j9ARGvwxa9cdY6rBekWqhUcWslVugMoccFTTGevcEOzAHxAROIL416k/7b2MKHEtjB
YadcLomX2dGpxHXt2VID0cxqqqBl0zhTNFfDs6RyjurbG6vZlBbcPNKVKzUgGnLlB6HoajWqf4X+
q9Wo2hX6r6aNb1aQJuFnV6gqOstpFCv4h5LR7xX+t1Zj+XesNgg5obAfjRczvBFYCHDi1Hdf1f5Q
5RUaWFkdgFTAEFcG+lt5ZVDGnZjzXSd+0xHttAOAlmZOlbnXo9MO+jwYddBj7m9n5ZS9zp130Fvq
Zof7ij52uKdts41WBqiW2yrBMHMS7OT+LXGXJ6ku0SLi1tuqtWl2Km3Egqaq4i1QN8cNOVWX9PRy
kDNeF2pDberZyc2ND8jTG0F4k36a/b0yB+Rni1g1qvw/PUG8vpJJaaUmpeVOyvIp+YQJ2VS6j9MJ
uqZ7WQhyhkECS7ck4bEN5S2gpzipVBNsVgDPEgGJGRGMlbExhtow88Sf2gZHWR+IQYLfynYAqWrK
bsWDxXgmIHyvZowFN0Cil0vhGN5SVHwelxA8qHXnDhdQza3vdhQHU6WKO6rBc8y7lbLW+UJCdAD8
qupVVQR3xkSTe2dV1dwWt+WMKUb1eDYK5x6MHibwJN0UPQmqlV48X91RvYq6pWYAa1Riwt4jFEkZ
qATaCs6oeBM6JCikVTMJaYyIPG5UyhfCSRB+FkCZr6llPESnWZXP7/fVKBz3YouGjjj2nASkhkjf
fjBrP73yDexxytAfNgFcGdIv1tdcOdKfcAd1yXYo3ALNL7EFpM/cY/S3Q/1FbzsrR2YFd1bmSzdD
Cu/YSeHq2rQfZG7O6ISXc/ZMtskFW2O6mKewCJ+nxP1New66bqahQuzN7SatnajfWfm+tE9V9Qn/
0VqRocuptkI1l81pV/tdlfVRzkHH7ZrKug0yp74+8YG+6bQvs2TGQDBbZhAOEk9DHjoV9Ss0NOoY
n9iddMc69GTJM5Ft4B4yGPBafndhZlS794jObOwcxXRAiv6fjsfTyRcw/75c/z/v//dG/v+VHniE
T0v92+by/ygYzWBt7KWiv9Y4+ivfcydaAf4oPF6mpI19cWpu2EWjG0bVleVX3VomjHjjThRyWsWZ
JPQknXJ2bzuEX8lsrHJ/Ph2H/cstoZc5NfsYc2nnzl2237sSaw3guCPaZXsTkTqE1cDtwap6BfN7
y8jEwe8L5k5s9CTtHiaqmwtu64He1Gnv3G8rG1PQZvoDR3CMU7Vx9l2wcffud73v7g43/bv95hYk
PaLBxbn+UL/in91dIm4rMIZsnPWGW/c2724Pmv1gM9jqfYd4qkUFWnd1ge82B43N73q95sZGs9kk
NtEU0HfuThtNdkGWHRTDKAn08YcZ0F/XQ5q9BxOiMGBpEAd0ILConzlMHdiW7S7YjsXXJ1V+spA/
GZb2ECzhExyVBBn9EbpwqtaKp4zSKYfRZaafGF1Fm4/qZrXXnsn0tBtbJW9smHgW9JWJsdSHgkr3
EB6xx4H38OmLhz92n714/uLNi+dPHlbV7bkJGW+sJLlWbx7X5yfdOOij/WZQ+w729+6nCb6xfznd
HTFoDSfd+RTkoCJCxnQKSsD0qu5nw2xQIqLMdNkVgiQ9fPlT9//Ze/XCu01JlSTt9d4b1EE9TtJR
r8YkqMYfDqELdN6dzJzk0RB201pzBWV1FaBfEuP8GY72KTqkbg20P5dylQfBg6QFUm5UW9XNMujQ
cqPWbMqvZqvW3Ko2yrI2QL3tH1RTpj/igdyACaCZ+VEcdJE35YMoZgdEd7gS8UU09s8sDCdzJsq1
OqZWwL8Tp5WiUY3jAGA0haETuwaI5tORR9XeJsau2cj4Lgig8xIn7qWJmgEk2LMh1FQIEnElU22P
qvXT1dzhelZrq9mwSlAkgkFFcyeV3Lu4Z0ksJQ5Uw632uVX6c78DYcLaWr8ohNME/WDg6RHZ0cBy
DRWVK+mu8MRN1tYQnAV8SL+wHxiHGWWVRvm9jEq1VSpyj1GdNiuHTWpYX8kGBYfbNas0FpuLZ3k1
TsvAINBSmYdy1hqrqnM4hoBgbEq7+giHhiibSTNQZrLBh+Epjc/F+zgIRbTwrvFhJ/cZClW76nD6
jr1n6Qzi4TqKCBV2iRronvrg5lJmSFRffRjM+0ddfzDwmvkwW98cTuswGfSMM4NpdN5lPNv1+78v
iDImlN0dj7szn3Xcc+4RtFZBFxf9jFcmBYGDpSPcDqwMJgVV4qG+QGGKUFi0CKoq3yXdmO0FlJtp
/p4RWhn7I1Wu1Y6Dc1G5LiO870LCNHOonQwRxZi4YufmQXQYOzuZyur4SLxn7yDhJK9oTROcwhDU
etpkajAczfMq5rxd0AyMY4gLCHmR3pdWYdCSD1L1DfWzP54hDPDJfkgYjJqq2IBVkopanFBn5sCg
LrizJnb+I/8w2/Or9PMz+ni1oFqMRCaLcR6s/GWwLDCYZDyhfmJOuIDWty8wECG8oNHaic3GuA2R
t93e6CMceg75DumP19ulgdOlVHeK9Pjl+vGv5l9uns97Ev6f3VJ9Cfb/Ev6/udFq5OL/Nlo3/P9X
eRD/B9Mv4X3mkT+JIfilA+ttczVWv0PRDAeW3L6dclxQPwkTxPEioAqHbBM52+CmtFJXz6dMy7LF
lqu7YE3lsd7gB+f+292qvEm0F3pvK279lZjiD0wYGHuRp8MJJcEqbZSgnYSTFs9p4i1AX9khXfyz
GhsAjvVT5AuAwxVzIYbMIByaa0N7pRcaEp75cxGMcM/Y8+zrqa/H8+Nu24WZDopwejSNgxTgQiLr
iLmUy7DETpc9G/QCe6UoyvGeWOervRb1lKiKIyYY/YkMj6tgrpSgD1t9HYfJ22tWBbYapP5cA5OO
b5q0H82gdASmiYm/QYdxQysfRsSxpIfan87Ou5yRxuoNqmBWoDAp0JmdY6bMymLv621qSJt3DOnn
LtXtuWCkIbEcHyr60hoLTbjj4vPqAvHRV/R7ZwU8YurPSv1pEwc6OJOIqtYHwI+7KcY4gZ9jGD+o
OtbM2lbecVU3cfjnxMzCY2C6RLCHFioFkSvhOEym6sflASs/yDy8u6C0tqWeFDsbMwREIRje7moX
CBYXWOaEHWZBsh3Nvbc8JoLkX+ZSEA3+JR4F+VBWl/kV/Kaj3i7xK5hUkPUu6J367HCtUrVOBrPO
BN9+jD/By/wGXtVZIP79F3Tw9/bzffu9vSjg7bV56MutmH9OP30F8LyCv7z84L6c17wUHP5JnOZp
rJkcGDkveraKtx/vOu/L+8LLzN9HesR7WylGMpf4wvs8/3fMPF96+NlAh8ksXdvxd00xEO1G+wtC
IMq8Xx4IESdh5uwrCIfonn65M+9Kh931hD5Mr8K/KnphHpN+UuDCjzqdLo3VZ/tyfaH6Phb3fS1s
9llB8j4SpV1DFLxidkZQmeV5rzdia3Y1XxVr5b10eVkXXcz3qnXVyl8u/AVoLp76V0Byxo24MHvQ
EbLI78eKxX5JXTncJ+uuAPn9eEVKP54v8aCmQZF4UctPwdLZctv+ohFn8a+JzMxCCO23MIk+W+Xr
NVc2A4O7CEaWiSBmvCT8rF0PKH+92Fwgvy9lDpjXiucfhew1qitwf/VxuDntaO2O9KyINkSHvxBC
X+YB2gV4XjSiq/oxD7aU5+8vj/0zG/SzA6R+zAFwJ9nEf1381JsbpL/+ce5/oJN7/cFf/+Oy+5+N
rY18/L/mxo3/p6/ysP6nkdTHfFnzWJTho3B+NA5AtnlsAfCrNQCABtNg0Rf/UFBrI3Z8Rz1qblbq
6jF0udtKFtORuetRKm3J8FwVP2z+lTdpSNtttVUwgLsqP9ZhXCdUX+RPBtOxmvlhFKdbZBOHZQ2q
MuFg5d/pqfF0oGZQ1CIW/0x5S5uoiA8GHkdXhjk7T7c4DvvR1Gj2w2bgvd/vv2ebAVgEU1Wgnyf9
c+WxansAsgG6IoQ2w0lF31YR2XR4NFvMlXePnRKmMsWVdJOuLUG6xfesya+NC2Ck8F7fPBgrgx/V
fmJl8MYYGTxN9OifJ1r0zw+S6IY/sqnceEHEzGwUBpGS6jxWSuXQg1Sbrhf3ObiEYgkiuxLFwFjg
Dw1K+NykMfPk4aqMR8N5JeNsOouThn3OqGBSck4V2x7E4vKLCsTaECamT5w50IZ+5xw+Hrp8xuuW
NKChrU0b9T3d6VEQjJJmPTNpMCKAbSJ3l717LTi0VTzV94fc/aRbRo0X0Ky7MyHbjgeDWzeq0rqC
TprVDtYlVI4ekrhYO1sVH+wD8VlJvdJ1ywRnaoe+MbUgHiFheaRDeelFg1XSdmDMbs10nEpQUP7A
UMl6YOfSo4E/92nvnASi7DSianA1l+ho8wK4NCx09ittdL8gfal3bqGjCj5ISLtcshvpwP1ojCDc
NI3LUhd/9spvOGu3F+zN23mH/7RSMFmMGR0S+fq4+5N4ASeK6XH36ZNnP7yGUhNenr94vgf0it8P
Hj6knxsKxBLbGmFb0N8i/W5NgG9vqiMI98WDT9tqhGcUd9Et2PJ6R2G1IK/R1Z0zau0y3vO0C9El
OqaJ01yF4wENHL9ko13qUoc/eMRZQ0t5M1cC2pb74F/eNaopTa7U07zgW+uiclv3Lvz63UVfty/q
0YwOwItaxveLej27sOzaxWXx/cKR/clAb1SukOWSfsosXiHPpXVcPCZnlTRbdy/I6KVzXtb/GVTi
L8rgSY7KJf2z2+bPBnFJ4Muvkpk2Oee/UubmVWvlKnncyv7culfR6risLcl760y1eX8RH2p0ls9S
uEA0j5lJJVLHE16tV5WdecYy3omORU+pm7g2u9Vobo9GZ/oPMaOJ2I41I/FPxTtjh92Iclv0+czY
J8y1a2jRtrT2Kmg92N/ePHCxBSOdAAriCV4yQjFRNBcz1Cob0wSDBD/5Czrlp1x2//aBgMbXg+zl
vd4LutWIjI64LpGbHmxYaTRnTjKvQ51+7qQzrWU+/OF8IEJMkt2LY+lsWrZ3xpaukOud219/8K+s
EjtmTB35+5uNg6o66um/kf57pv+e679/4G9anx3TfkSd8itV+d2TfvPviMh785tIrDPz+1zILf79
B40xoyMvt6KYjvsdaLPmVrQO1O1J8G5aVM+evH724M3Dvyu/cytWPfwD61T6AyDjLwMVPwiI9OeX
Ca089Bv9RT/RP/TryO2MCELcbWGUiBuiRAxL1JQKsc30m2T6zWb6rYIl5AWsUBzs/3ZQSaQhYoGF
Y7y1sbnlNMfr8liqOkZVqOnYnURZjBCg6vMdBHIv9b6TXZ+2xWB0wXFvZsJDttvqrom24NsjGmiD
/vN8+tw4G9JTkRBdk4BW1ayolq1truZehfvoHCFeL1WJrUV4RUICmk+sFNW6uXmPqyWEQd277VTM
WGTHLhwYz8GjiK0EU+JsKZlwvbq0abE2eb010lyF8m4NHLZRxb8vYONY5RyWscPLOKTG5lRelpvs
1KpC2G9jfu4iUyz479nIo2FpKHCcn43dLtjkH4ngLtvtedTmLCYDVtqE/P/MHnSjcWSqczDitdRn
EelH1/YVccHdu3c/ChM4h89V8UHh6terUdZgckOHgTy2uobczHS2ZKmkheWPsTWFbamkmIjUQpEu
sIpgYXHmdHLlnZVxWQXEEeWKZ05ULmxMUYie6y2DAUxL+9FU78zll3SpCf3uu51kA5sDX9BaF4qn
XjJFDqWwJINT1Vm8f4/W0Tn+ZJans2Du6QVDuY8PLmv8/PJM0hKrqfItTzdaTMxlULJi3f2h1ukw
lz3Co5vO7j/eFdGLHBuQDNUWMweradOUeSM5p7QZaooPNFDuI1+3Gw3mcf/ivizpRmGlzXyl6d41
l/RuKRjuLcMUF8yX7ibe9Pws6W7rsu62lnSX5zKYDNyZtG5KLOV/UV+R8deOdLmgZSrUMca9DAZ1
hyiBnYIzV8Se7B6EkPGkg9NUy8y6EyLj6htDR6xpk0wWGjoltFJ5TFrZpSbLNOjJcUczI0D57le5
z6bt3QxgluzNQe3MG7jjN6OoJB9b+MgKAJO5Mary+ijSzxRx2zD5ULpvSxfyRAxdxX+EOaqk6YeG
MQOt1WrW/1hGqIpvX+Jx/fPbvh+/ZHqJJVLHj0ShWG3S77cQhyJ9J4ddq6KK+PLFi6dGE+QnFgYX
aoEUNPqW6kVpIqVaREptCmoRQW+b5ZZaDMqGCIkcU2LdcFteha/Dh/7CWBxKsixm/n1bsoyCuVYF
+NOUTN/Rc9iGbkX2FgfJ6YWHJjVNZ0O8GOey6lR7lYw/zG+zZUmiIHDsKI8MCeopbYFjXJ0eJ81x
BqTxjyL+wNzPohxWt57KNZZYA6AsgxevO2I+TJ9XCZ+aiJ6gJs8zFbs6O1AGEZWCY9H2p0FDiJ+J
HvjWrJyKWlsu2OAxyqLSVYnqULYyFtGoYwnbhlxFAQuPL23K0+uL60hCnubiHsLu0bUzdzSqgYJ5
EbBSTBLS2430jdpFto2FcKoDVdwjdpevR8ou9SrLJFOZBWq+NuOmwOTAHBzrmqWyVPUiEjLrYiGY
H34WjoH5F1mpwiQ4Vd5t6dP+4qCSTK+rgZL0m/JISMQZGwrIjFNv9G1OnDWin8EFyeVLJsvIJyMx
yKInI+lhJLx+MJxedjh8e9Pd7zmd1CvJQIxzLO/lxauxqJtsaN617SVTg3S3IaeR4sWcRjJYuRW+
MUoqp5VqopelIP2Rk56idRtnfq9PKJTGaNI9RjZIwGJryoV9GmIOC3MJfbyTa5o7eR6mVbbSw+av
MnYdsbIqr/vnIUis83TB9FJxmDhBhpqRe5fDFc4orkTEFwHxDgfDTHaHmG7k86c5HRlmvkdOrwyk
znKwyFeOR+BzFjqy3+xzut/gr2GVfjYZkGE+Z878q6CKxfamByGhroZfrSD4dL9lEs/5dcO+6jwF
jZaWv+kthtiSi7zMzzlpQQ4nrnei6djxtzOfuvC2O0a+YfIp+w79uk85sWDm2enRV8TePNN7mtIZ
h3OdL+sbHcIv7YUxjlwdLjfU/oWEonCCJIvZJn0ejsLDozlrH4wGcLDKWeCUECiGCztBdnVoCkd9
dbqYuyDoEs+wxOdCIfboXow+jvJbOL0THWYq2SAmpqzwdagDg+uCLvHoKHF2EkyQwuzeoyHRRxgY
FWFjQ75YVH6bTw59LjAAOMCTkCAI7nSwDI06ne9lOv2uCBdaoEM416vzHKaChZreB9hCNU3aHaTG
n/9cFX8yRUsrpauY8jgllblupwq0+rPLQNBg1cWtRTjrzGCjfRtjFzihciBXI7lPzQMXxIxZ7NUK
briQo8poxb1ckS8bB4JMXAFhwhKzby2oYjMuukIew4zllEALMp9XiINzUVaasM9og+b0QDWuSLae
u2iWLs/lS7N4jWml3Vw2s/+CU86WUv9dstCr4t81WeiJ8IWXYIcqW77wLeawE59axUtJST4sswtc
JuYClI+np+Myf9S0WFR9hXmBmb2dGW8uJgNL50f3lwDbBb8F32MeBPSakVrkkOhFsFssskeMIaBv
A/c71EYhUZYiTMKJ4NECOixZSTTWujHrSecL5LjXbChRhDmK7QpETRFBk0YeVaH40mgjTyRkbm1F
0GbpHodELCgrFBRHtz7LYuWCTkZXoLqkuYgprij/ORDaKMIGEoooS20tpbQKB1qMMovQZUFfg4RQ
w0VvkJBokaXbAiHUUAVeNuVlI7PK0rQZ1hdMcPOUPH3RVq0Vsz1ky6Y2dCaAsRWSaE1zK/9wNNK1
wYJhylOq7AkjnTDa+QzGs4hsbtnRB+53CZUsvXbSrQRBpj5rGpUXiyEFpFqXTxiHyIJYQBz6TazH
Y3s9aPz9iXs71sfUL6KRufwWgz3dGbVOnlVjdwAvdxxnVX817CPRptrFXAjVyFgrdO6HxPnJz3X1
pgrLeEaATmKlrcSluZhSv2EfH5PVOZGhJ8ZrCZ8m3A5fXMpsTof69pKv5RwDvAQBGkY0dFu0n80I
2dJQhqHl1nHQj5P7VsetRngYX1l+o6Vd3GxFy3E0aW1FLZpiSPzrsQvwZKS96eDcGeU8HLiIUZfW
rjHZ0SDlOMgf5Ck5ocXD06GHCqushppOlFmqZcWFLPK8T2uTl6VaxI6oK66LgJIq0/vN9etIw6rn
HP4l34tvlXS9SGkk/Icnq7eSG+Zld0dOddm6crXDsQPvmOXNLLsEsuPJ3KxwOhYW5ojK6suFVN8M
68ULbU3AeHnfXLeLehfOj5L1W+QMz6x9zYlQdj5+nCo8rL0kXr1beRSMQJXQjuN5TTlStJtqxyJl
VOQZsShlrv82DdM3h1foXKqQnoKxfxYLfo8X4zjB7oWX9ek6dVGeDxr5riR8bxPanKArXrMZ0whB
DTJtFnG/vP81AzzIcBSDKqMUSEwLbs/Gi1Gc3J9ZPGIukGQJFF2mMVd/S+Pgzq2FdbN/Szvejvl+
DWPq3KpvDvX37iTu0u7tUrtyu4YOcApnO8xcpOncfC5xrVJIR4O58IZNn1vOmaVPJwG6XLAx6Ne5
E1WBxXrqO8/YHVuJyeq2VnSrZnRw0rcumIaiy7WM2Zno6Iz9UDyGwqVk1XW0abCznmT2zem/k2z4
rncqzle/zq4syyayRuJvUjQc/TqcSdrPVS1AbZhNn64DakNOFaxFZGvgj1JBJbXQ5ORnHf6O8uvw
9lgWnf4y5M+0lsqp/BgzTl3jLxM5bUbaPVpVvZ3NwEobOocosOeycEggyQGt9kKh4ac+ucb8fl+3
Ba35tqo1E5gOxZm0pncWk+MJTD0YRtphMX4XTgJfYmMW3mWODNEaYRjrKZkkE9rI+kweFqjPcNX3
JXEXsUycTmTKpfVmTEFOvaSkqzBjylFaUalMLkxZNtuHAghh4xcBqGeIYT6DxBerWY4wS8FyxOcy
IpwLM14WP9vOQMQ1uEO4JfUzhQxXzRb/Op6ybUt4Q0sNNCO0c0rI7NQ1V+LN2U6ooYer3E4lA2DK
vysfLCGtsdYtXOzO8YdNp6B8zf7BoRQwESfhbkUJKCdTovsIlnm5afbko1r0qcfg4YO+1swOK81U
uFy0HaPcQlYzwi9niWujL2edS4pd7BDjCw3j5JEUk8etGWqMwL3pKbUa5nKYyM2mPlnKRoWEI8zc
ivV2FZ6edrysItrydhXtlL6VyLnqJ+8xSFu7BRKO6z5/2PXsuWWWhzm3DEPFTNTyzZzm0NnJPHWn
gpY5R5V9DWeY4OQj+xwuFGPlt/9FTXGWpW3pr1dqjDHGRU1RhqUN8beLm0kqAobJ1ZR8MNV8y3GQ
1U8OBsrpji9i/zBoE/kjzCW8MhEDREvIWjET1ZImBlogBv5qc9t/uidr//0X+P9t3m1uZey/G1t3
b/z/fpUHjniZ4cCxk7Pz9tVD3HxzmKkIPl+9R3sPn7x+8uL5a7b3bltrUH1lKXfmjl04RwF6Np3M
D6fjIDo3BuHSSketUTPKY9trcbjrq9bWdq1HzM8smg7gHP8I8ifJv0ZHlG4ihj9YeoNNHDgJyuXd
R7aNbUrj7FX1Z3PjruqB5+LrUjSMN+X3picBB6v0fGIFYPf+XcWEDCXGoKornSNkJ9d6l2rwVd+P
onO+DmBbWCrrD+j8R+YpewaeIpYXEBJf62q7W3bJOw8n5zxAAsdwEbG7Xq6uIgFFoXpFp7CY3Poj
4px688jvswEuNTarcvA0DLoc60GjOMyLdX9YmRuHO7vkfTJhO+gxuw5WkPbRd4arvnH2jA2/RM4T
CBkD45BvpPeJK59VzJQZze+24yWk2yW0jIa1ziZuD0xeItApa+KOuT8dLcYTpsfUGce22+U+63mO
wSyO2Zi5oH6qbEGV+vMpbDkIhjIV8BnsR3A3bVplNqWtgrPZKOzTaLc3edCcvLxpzABNZQ1G4jVd
NZvZKw+xKziluyCaybYTBUM7upR3AqNFWqM6a1RWDM9phVPvI2tUHYtXaY9DbcWYgCNYD1sH11iL
F7g4Xhbnyvo5dkyVh7PEWnkhtrla9mFslheyF3NQ37HhexyVTarhGFbKCARZ8BlVHb9UaTMhKlCz
SogOgimlLBP46kIMmvnilba3KxFGdUehtihW7+mrG+9IajDGCcUWDonJtN/QNx6wifGb5sVef/TM
d9jJ9Mz3XkplxTWAbnB+NotoQL7Tw85pNDOJTUpsJjmbNmezmUls5poYhwOTB12hyivQiOTfiB2x
RoQW9UJ3kAF9X20w5tzezFZGeDNVF9xNvecmis212cIcveRWmroVfms2nDfU4PaAHRBoNE4oBZiU
sDvHibOo1Fw9MPZttlkRLYXX2fO3QW5qNiIO6x+EmP+hsTgcceX623DHR4uGNSUfWpMyBk7r17ut
XMFmuqC58DfFbUFiFwLo4yTg5fK280fmkooWG04RYomp5FHedv6oiTLUrgN5llbSB3lD8TUO4XYf
iWbWj5qp1Wgg2GpzAfTyu2xbQzM4yqGHlLQ3dH5zJrQ3dEfunn/jwCccJlsZp15VhfWgzsf5DtWh
Dz4lZ2fhuWd6PSHWzieWGdQ8HALPiGkYqkvOuKrx6D+hnDUl8JUSTExwxyrZ8f+us5lxa8zyO3UT
cuPfiaWM3aBlFqfA4uhfBKXoXZkhsqr6dAbhJr/aoixOZ7IaYt8ZyCdrXQxMHKxj0FGj6WKmnQyc
IgOHGYsSIgupvomqpStOTCYuQTvSUjPfUitXtfNxQ3/MttNsprV8HNS0T32PmgcWN+1TC9HGgdNp
BnjUkl0D14cOGogbVxqpVLGRVNF0qsgDKxmRoBn9NXMQGGRgsUS2zzEfIajCQQtX63CsO1jQOaph
LdslF6EM50GUQiv7VCImCAveaF+ONIrHYUbgAOfjxqIxg/vxe0ufcB1VtilqS2JM2zDGUi/AHWLs
+C+CPfJ3MbQ+aRPTeRhtAAb0XxOgof+aF+78TH0sJsR8pMhtvjKWAhYSBsVU1e3I7vlcwb7NmC7Y
lILG26vFAsm2x83sslobVR6uQW2XdgFw0ZltN1KN0+7oXxXNQIeAt4eKWwc58rCRxT5VNbaTaNDJ
RePSoB7zsOLGhcNqmrzJqGIDCMZTBvOOrwjV2NTYTNVF9eQhRBQOpTsETmGF+K+lAXHJcNB4I9Ws
RVv9L4GulnaY+zq/Ul/nzbRws2/w07x5Jczk2jYkfl90ZWegw4iJ+55+1fCjrc5yvBb1jH2kGF1h
MQBBLRgxlIcJLLM0aXxGy+A8uXUDhXlmRfWaMpPiyOgQdoZnnLljjm03Y9PNYvLNdjOHgNnm3gGF
b+zunTRjZ25V8Io0FJqtu7ip2eUbm1otpRiOMgZa7FwlfR0kWDyERinh2kxmP3URZ9xfF11rE5//
KdLtRP6LuPVfxP3nJfLfrY3Nzaz8t9lq3b2R/36NR/v/9FV8PiHEBfvc3iI+VzCDot1DawKByDnw
lgQrXUTsEnNqwqBJWPdwcizhxjwY54rZY+LBMwoQ0UsHj4ME7dL4cVUbZczUko+yprlAji3vi4am
QgR7dmQaG7HlSxtIl3arGzMXg6YTdB4eLqaLWILeg8d0fW1i/AF7r0xcOLI8Atvm1d6DR8/26uMB
9b8PyS+C22sfis26GWNbnfqjY1GBJLRCPJf2+czBfpUnibF1TEkZwwinC+S3FccSiGGcLqu9l0+k
mPXe6ABzNdYeLq1ZEdyx4xwDpBBKBWAjJMNlW3XK/VvQn9uQwyHG+0MQTaaL0ShEtDiOiMex8OA4
Zx5UJbY1hjCa9jmMHmpgn+kQLnqEyRYxZcZcsRJoMBA10aFEn+eWN+qY8LaWQWMmpZux8qYRbn6p
dQwANxHih3JcYR3+tvrv/+G8/JldzdeiaS+csEakgLGaxGhPEtEfCzFzC4B8iAiEbNr1KM5yTDvk
/zwFMQyHYKkZYwHJlGJsIxi9MoTowNpAdtRkYn0DPiazyWKbDwzEQODENsifQFZ8kibbA3OCHcM6
WsSiTFhkTkeXEm0EJEAnSupJSf1FfK041t4YVuC+WQ0seJblNAqGc6jHnsd6jezoVUdV0yJnxW1M
ZyzhppGGFR4M6uyTFmEfgzO/DzM7SMV3qPPUwjichDSGvllZJ34UIgLLIe2ROLkk0KMygbA5LQ7g
gEnW2jgUa3UOyRMXORp1Us5nQXce+QT6/93+RyUwa7H/Ua0Kk0QQ3HFjEYpnUnFF+k49fdt9ILqW
9OsHUC1QO9Uf95ryaa+V+fDipzfdPfmGn3DV0JSf8GHakp/P6ScxgHv/Z+/hG/q5ifLJrQB7HnkU
jOa+WBK8W6MaavTf2j36e+9DIi2eQKramy4i65JX79aprPSp+pU6Z2JU+uLPt1Lox+JNKHos23Be
8VT77uVeWlcWcNOqQ3A/HJrYg+mwT5TrHlDcb+atuZ3SX8WyBHDqG9oFnPgZOhPdHQyr0WaspTfi
Dji8jdb69uY66EnEHP19QVhzfhTS5EkHEldv7NBQM9Jp7RzRxXFUcTjBKOtCE+uIHZ11RK2CMSct
+BnhxiBJHE6nc1atMElix6Bh8kouA9+544XCqcrZYDBS1Hq8jMz0bzkLAu2i0ZwEEjhAn3v8Mu33
uxA1Ji/+eCYDzbY0O9JrqMGcEP6v18+b1w8JOfaP47apvMoYOzb9YKKCB6iX9qtH3R+eSaP08+WL
p09lcdPLm1dY3EpXnRAXcZuPBEPA7LDjZGJl6CjSmE0wamUHdBNbveC2kXLV+CVrJPL0rdh+PHqq
rUUe/7ckvHpkXKm8NBTLR7lTefoW6j285cWpysbOJaV+pFKP/5tyv2VvdnZPJkGBV02cGDb2Sugx
mSFGOzC4AQZgXXPnQrs7v4+RaXSVjv1rLXV2d5yantLUaeucwuoePcUA91q2Ujfg7gUOVXPPFbpq
AhNDJmM6LGm7qT4/5HV3Ua+X1Jyt9bK5Ot4D4fxWwgEpAwo9d9zNHTGjsosn7x8n+SSCJPue95Pj
1pIwtswBw5ryDt+dvJPfFTHHqupPFTG+sq96azrfI+f35DB5IVogeTkKRoPuh7SmJLjn37O+d35P
m5HRGn44nQyJyI8M+TTGVVP4RxCzob+mr2irSYZYqEUeBKQ4JyA+ZGdbBghb2ziQ56BGjuW9dvFj
Jko4CCdolJjoINnYbtHRc1v1Uwq1w0OOcJQICiYREqQyiKT4cFuq/skmaeIICAXvKG8TgZbqcoat
0S9zoOkbAsuEgH7WhC7RmEzAr2nXA6DnuMyyVoUG0u2iQWo78eKZe6hZJrc1ka3PeUtvJ6qiaQdE
jrZw2sOAiGboz31peG0tSi8XqWFN+sZ4Ku+ERsMI8sqlORLYlXJVW4dGXoK+st5dkq1bdWCGJl30
kSnVTOUtanq5PxkCR0bmlBwOlUwFyzwjmSXlVPQZvpBYrmA58ZTXIn3oWMqkijvJY2LKldc7n2PT
at7g9Gg6CpJKHMd8s26dkc5nrZakFgLNPbOHrmPZuNb3upGuKLAaB1vTocfVVGQT0b9b6TLYn91T
TcWic3bdffSyS1csiKC46iuvznSV2iQgRY/cMy6/Ll1u4lg3BS/hWB3Utqa26D+oB7Yq/OPOZZvB
RXpEFUIqC/EBDpkdTezZy27Trrbrsw3fS7pV5CPMsbMocvPF87vUxVd29mnLLPPvZSkClbiZ1UtG
/Dum/HodH1RkheZ9QFx9Zyxz7LJV6BMFT8YZBViAEMthk9ZCsjNorvVG2snVoPsfYWAIJZ3yPUYn
tK2uLLQ+2wewvCacZH184WGdmiV7dZDJv8wRmQPyxBHZcXb8xrQ86fGSrVjVHV7ul+wzNjfPQr4r
zUwHtIhqeRc+AQksdZA2AeVX/ljvaBefZmnYp32mGaRS6Dgt3fJHHINpawdtMY5AIMaN5DcdOWOt
3RCoSrjAU7f+WOjjl/ni6RAp5arKV1LVVTgkn6ajC87fe0XHryCFTTvHGVeE0UdWxCgpStcxObw6
NaBxKGqZHKaqAdn/CdVkzfLxMNNQUFdze3ldxE+WUe4CVFuEITMLL11gKgWmVGAT2acVO32M0XCE
TdkJhwDdM/REenmmnPX16yweuqMaZ8YIsdnbQFyQtcSdmZdwG1AlUuuacxAtWlZ4itQtk5huDdO5
H6FXFwUbMDMmORtF2wKS2ZhjSfbrLC7b7ahmvUG4+k/bT76xtm863x3VvLe5uX13c7Nxd+Nu47ut
reZ2c7veyHF4iOTKTINcweAenqeVGTq2QHj95in/PQmjOcRtffrucm0Xu4uralHYbejHL7N8BCMO
xq0ukrfs4vkoB3M2Ny6oqebvE+fQIIiq2gdmehZEJNXVKyvnnI5tsY4khoOtDUK5mHoM3z6E+3qE
cRApAJNeXDsL1D6y7uYldcuJ/ZGVti6ptNAX3+XVbiTV5ovlgvd68NPVbLQ2JM6GBO4V2WWRH5x8
8F+sF1cye/s2FoisENC1UmOeoBKpy8l05OM6SDRbOTC7eChqbjsa2EfT6bEaL2ih+r1pNFe4hs1V
yKXZJdLvS6mfAi+LQYzjHrd67EZZ5MDY6iIFxi8jA5aA0SwBZlygESCVYK8+WgRsfk5ScYElcLA0
Y7di3m9dVDdyaOkHzWrdiqN1j5DmiKVt75CeCKhNR5GaSKpTfXYOvnoivzbDqJpUK8iWUbnqHFrM
/tgK4ZORaKEHU73iCNGwiCLTFg7NSNHNG6iTqhZAV40PSN2iq41nW7yd8Nn5YMvE9LhxmRkf4STp
SsWOjKvAg+ASx5BLeQct0Uo8QWYc8fEZM7aiL2YB8rbsR/ZkSHlEvJqLPos5i13CuZSv+OrrX7ez
PiNJS/vfyx8k4hfLYfg047G2ZsmKgKh8px+f5CMvjfOLoZKwJF/Gf+ElIEl7ndNy8OYSD3uX+UK8
AoyXOkDMj0zhygR4Zz8bfosXrPCzHmfa3VWsobnFGuhIQeimC3wi4sFtlFN8KdIuciz3EQDJBXrn
rDrc+5eAnZZgM+wGNibFJ8LuM+D2eXtH0zRmMRTE/LHbCdhO08jZ5c+U5Rj3RIa4xNXyNwUo1Tor
vJw3cYKJ4qQTMj9PrOSpfhDuEHKy1tRkESxbKDys8yswDplJZ1IGke+AOwTN13JL3+YesBs06+HC
885tMDI87LB8e4OjiW20CogxlMeAqB1gzoHrJK2gcziRcQXDk/F9ul1pgtqzzRGV3lrCvLHxXR4y
VbGyK/CDLi13INy8MNRrURniq6i92x0XMOncLAO7zRRkRsa2WbAofq9Da8srpElpqWmqKb/pipfM
h0IIibfS3wvdpxuf44ZTlNl7nyQMEESDmNu7biKgwamtPHid42O5HKlgKvIZ9DjdRRMOrI/kq3hs
p+w8Oujsw/j1fcbqtbisdrxOhfPfL3HgzgVH04Jy6XlaVssFNWjP70fhRVNO1J6hv9PZGEm7mtIp
3uPzfPumJ/3VI0yoVv8oFuNciFvRGDS1Uy7fi+B1iV/giNknVGZRC7yzzh0nfDk4EIPnaj/O/R5M
5EU5j/biaqxeyWUJ/HG2zU13GHNUb6gvznCZPVxMrJ8BTzQ+sYvqLo/C9bzxoQGUJvLvUdfhqlzj
a0OfZ+k9hnVSCYsx6M27GOD6QCCY8y+XS+CNn1t06eJyQlBp/HALD+KcxCXVW8dBsZITAkSF+CTn
NyB5ghK+om7+yF4++VDZVRvFOxWA4vIOG0MlXMkf1blLrX6v1e7aaOS+TYB2B7WzSz35XqvgccJ9
m/CcElgLrwDR5Dj5HT3FGS0TZ7KP+fc7nZPW2wPRTBU/oAMGuL6Qi9qifW0CnmBrdX94Bu23U/+c
4M5aVW1xfMAKwOGcJbTG+4Fecdp6g3cyo4Fk/11hF//wzLoYxgZzUUhBlEFb7M2rIh9ILD8m4mht
TZ+lS2pObUoNzkRaDPCk17QjXDwaRI7ljYC7XrROPI+yYm3dhaUIETYHO6ZFF2IpDKIiV4zpjlCu
H8WF+G333Hc2bjKC4Pf9rYzOHavd5bNOF6LSl0iftCa7iqfjQOtUn/oT9271kuvE3Kx8E07gTNR6
lL70uhEEl8xClrSEqh3qEr/o8KJdwG3Q6KcH6n1HNRc4hsN8Fh61zTBdduJx55G3aK3RUvzGOYdS
brKjTLdktRTJ2rPBgkdToED2WW90YLOu6zOcxA6PRv4F4ch/ke9iFpI9SugXR87Zn//h8YiLKX8r
QWKRG+CcJzn51gSC0cIrE96WFzIkpg2mnccY/J8NnqcoSmjdaXbmM4XD5cOjer9H5YT+sqNMt+Bg
eXtb3aBiYQGDX+y/P1mqS8BUtBlqtQLAFVLoBQy4pUUgIYzYtlr0uAlggOEG0ajTAvVta3cyLaKj
ryCyMU9aTqSlS1jWB0vCFJhHhyvAOIozJCrz93/cFSlLVZ0ajKBlIUWn6EUkdbrSou6iQ5WrNVTA
YYlcO4NglnWIJd6gTU+XSjnw6HVV7Lo/dULWauZEzGcM9frjtSBzH6o+7DeMJn92gWiNv7DtGsfA
+IuV+nOjudMvnH2XG1y6WkOYDRxU9PIvivmQiE4zwv00vFLIfBn9kRG3WJt5GNZcPUyS0abhYu/N
9s9JQA38v0G+C86ND59A/3xjpvuiel3ax6VEAisAM6RIQeShpVIxEDsdZvcdKiMRoVRTjOVV2Xg2
tzV0VKuBk1Bu7gr5ece8/kRdGs7IPOJbyazIEyPXSVKWxj1zwpKkW8nu3mX7nZuWiCBFsqRLZMLZ
0mDgi0iM5LY/5QZdUquKwQtB3AZLmxA7iWqGmsJ3wcbdu9/1vrs73PTv9ptb0FX4Vdz7Nc76rV7L
DzYGrbuDzaC3qW0DTZtra/r2LiWCFYJX37YujWJWyLXb28a1ArFXfpPpG8k11v1L9QF2QxDMSmJy
lTecdQ3XlNxhRB8RvkRT6MWhSxzbiKLIJSkzhP7nxzYx/QWzf8dRPNJ9qqo7oj+U6qPdrXe0YlCS
W6v45LKzXo7V2jGf+UTRzKNoSzHXGE4cc1Y6TobEkAq40s0n6ieFRks6ukxyX+2YLelX59Y6MVyy
F9eJ+ZLzpq+vC6LCXGjwg7/RYtLV1pGeayAwPHSsCIxZQXhx2JhlwWHEWgVu+qxlx310DJ1Cf6gv
luNPx4TRJggcFEb/zkSFSVK/QFgYQ4Knw8LYJu33grgw2pqNFoMTFsayL6f0bz5cjIR22Gx8t60/
wKYUgFQv2213s8/NZs+F3VHv35v+7SrUtDQODzxxSk6jl/iVQsukTFPUp4eWecna+Mk73nIRZapY
yS5qZcjreCqULXWqc2PC4xGMkQFVOjoLSe5PD0/DdWYD1FAn6zawDNZMBnRSKKX+knw0S425Wima
BfpVAt3YjhX3qiAVtj30ouOKpH2NfH7kG+6PHq1bM9Ru2Co0Fw7H+khxLoM3tbJCBHWr4wNVS4Ak
KU7FgY20E2UqTYkOqOYxvM3Q6qG6xxXZtiZ08fjALBxXe8WGODBC0K8UeAeBR7ocTudTw+/YGj4v
CI+LFOdTvW2XRuWpspvbgPFDM9iA6TF0NI3VbkEfioP2ZAl+q82l90q4bKdoQ2sdOUewRnjgBtNJ
Fj6dBLImjWSCDcbRjVU5wXRMg/VECpmMjhu6bxO+l4S2SXBWpxn/mhRKvmjw6X1hAxPpd4lL5Gw3
HZ9Id9mm06RofTb9cTJgzytzo9PGyfJb0q1eG38xb/LN6LbxJ/2SbsvouXEOaxyDL0a9jb/oF+eL
VnFLvkrC1dAAahFEwOUdDJBRPaN9m1lpRThADtAEDeTZFB2vaVwcr6mfrDHt/KVwhRm7RniDTtYT
aFJ/NMpXO3eqddfpnZxJZ1IvLgAuq/doOnPCSzkLxlkmepgmvBTrgt+KociAGBtYuDroEzTOEWlK
unSFmFPFwaQYyZpgUq2hSaauSjuZQFWZ3Mn7BWGrMOzC0n6/T6MIBhISy9Dtw8jvS0ogO0Je/JPD
Lu2R37lbmRYMkV/UiEYEXT0QDcHNQ3aKm09NB9MqCqWFFQG9nu9VmdVL2HYJ8Zjkk3g+wS+eoqol
/h3C364pHX/LbWRpqK3+8jBcgHA2iBdlp+1hAnjx0uNUty1nJRr0A2w871fdT3pi8KWggF69FmHx
PY2TTy9rqdZtPIuQvndLabSVadPJzpr19UaqKb0MKnr4euZTjdpTYN2Zj8KgZaEhe0HYYsYsHWvE
pppEaKw6bqqE/saULyPb2fejHIiN3OkJPuE2e34mAjr7UasewD0JfTfmrvqXtUlFgmNwiteUmSgS
2MiTS8plMH6m9I9dFAThCLfVgQGUfjHNmbSkRZPiNmrS0K6thZvGW2ozy4am77o/JjvNr89lEV9I
j52muVHNjt8kOjAwSSk4cGK2ZQ0aW7GAx7wam1J5xzzgV8rnn1ksYlWSJ6CMX5b0MaAtOszp2nR+
t5zfGymWxkwPN2W0oQVpSxIrArsJsmXdFGIZBJ9WVdKPdRVXna7gNQunpHNu5g1+TcHD9DIX3+dL
BvtjKZ8mF+rGE5AVNiRB+vCFRn7PhuirO56D8vnNR8TN2k7KaFLXJxToj5APCZQHToZsJvEx9E02
0holI/wZR/OrmvPEFtIehSznaHuCD+hF0onL44CZnFeMBtavW5dESQQ0SUo6mPZSlOTj9BqnJ5ld
70VJVptadtGtnrX7qgmBjHndVa2tLSuQkTSie7S3+cifHALuaWu8LPYVAq5oerUwxwFrmItvd12x
7dy4dvoUv1J0u4y86iPj25nzzolyd0mEu4+KbueMjEWlo1O/aM2PTgEVnxe8n452ycWCVlGpwQil
giYXC1oF5YaFO4wvbFE0mnPR4Vm6qEhbo4FywnGKjTNl7o3z0ThFWyppKRrojETAiIeqdvobvCGa
r+y0KvN9Hpmvb15lImNKW0XRMXUnOUCm8npjcbqIcG0V6nhkbOKTeH5WJnphLL/RKbiMwQj/MuSY
6UBLEtkPU/o9pg00b48SAngxwnwggWanimn4nsGMFAK62xsLxOMAIPSkOmhVNNkXhMfVbYO041eu
a8O8cj+S4IE/b3jHVZXIwSvqF2mFcJQ6blv3u4mAPiM0p7nahZfmAgKddyuL4HdStYK8vUrNmOdP
qLt1lbrfvLpCzTLVp+G8f6Q8grZLiRDgxKngg6raa5pIfkjecJM58h9St21qy838nZvMmd0Wmi3x
W5htornlpts2mvdscqqRVtNNT1r5YOIO/rzhUhXNf/GogYn/598XRIquf4k2QATc3dpa4v+54Hez
gZCAautLdCb7/Jv7f87Ov3ZAGk7619fGxf6/Gxsbd1uZ+W/dbWzd+P/+Gs+36vUR+/Ptnau326ux
mvvxMe4bEHXJi6eLqB8MKnX1nB0ev6WkPnEj/SOifF7tvQZBAfk93BHDWuFHOhrC+ar4x+ZlxR/q
pW9x/qg3D/6mHr786fXeG9UfD+r1eptdwOETcG/EjceB+FOeBdGQlal3lOh6xaqMCu5Lvbg32eVM
bUU1leGeboW6tB75p/X52bxe2vu5s/Ju7+e2ZlyqcCMOYwZ28tf3+0cBcUsxsR/Vp83aQBJGU3+g
Uz+U+PqLjznxbD33DzsrTT4VOyutHRUfhcM5x9Ix/a71VXmFyfSk+6p2VlW1QK3s/axqU/r+dh3f
6v34pExUFyX8V5kob6QjfCJ1vaxa8h5EEb/bHkT9zsr39o1vg/nfzop3GAUzVdtTq7+y4Po9h3Ot
qNV0xe+pq+GISEIcekQQ7quVqK9qNAkNdQDOaF/V/qAiHFdZHexI/Eqch0H/iDr/6qfnjx88eapW
CBbcm6jfVit3yjscglfVtlL93lHI/Jp65/EPluVVduzZShUPQzucWX+H/qPM/umxqj2uqlVueGVD
/anW9YUN9IAp4RtKiYLhOkxmYAbnrRCh4RO5uU7kNd8yeD6bJnVUmaaj68+nQk+X+RcISXhspw72
cfPQUTStH5zW3KVyDY2EhY24S/AaGhkXNlK4tK+htVFxa08fcjvX0sLFTVzjULKQ23v+iGoVpkZy
dc31h3j67/L6sG99dMW+jZrmFQ05JfnVlpSPOmvqVqC/Lz07qNLCsT/Hyc9R8pPy8nAkr/k5Nj+d
Wln3UcrhOmk0qmC7uynjSsUOWg9l1O/ydCaDoxQBvGbfnL6MbBfVh9U0ngO2EQTCiIPRi8bdK7M+
o6UgUDWfvjsovEyk97eEIaDt9fzBsz3iW9nbqliySYhd4jTxqUNHQIlzshsSQYZortnZ/1Ud3EET
/QXh4kFH1YYtmM781SfvP8eTpf/42KixdkF8XbHAL6H/trcbzQz9t9HYvIn/8lWeb9UbTfCdt4UA
lHDgvCbYns8QgaAQ2R048kssLLwSvQdb0sVk5vc5WERMO3l83EU2WkJE+/UHtA9/LAPdBGchCKY4
IDqJFcOUd9gn+uMFseK1Fxv4p6Vq1Gz/qINwFydE3/RH/uQQHyrlEmJaqdpv93BVUuMKYiadgDT4
tT6aHoJ8us1i3nea2mk1cnl2dF+ACUaxeG0uSccadRuC2Ylhk43zrLxm49dtNfPDSEelDQaHAUte
4ooElNZlX57Pj3CPJ06kuaVyCXLQnmixEgQwbP6zwX9k6DJy+rSjBjCj4ZLrK1RKdikse46C/rGW
xzdAG1ETg7A/V2XuvXxGibJ6+eD1a8BkeQ6QaCVEGS9JS9Iv29ZgMZ7ppgiZznhQG0zxlsIxmwvF
53GJQ8jfuWNDPO+I5xafvf7zqBEhBlnrHKtaHPTQud2rsg4Qm26c8X0Iy4rP+GiqyxEP2nECPZGm
XNr4rM7i+YhQWFG31AwmdFFJdANXpYvt1aoi4K+aWN9VeqeClHIaTSeHq1QlOoPV4DVxSqJWiYlO
bbGefKNSzsM2PbsavpC+5WCczclwlqXWrIvva/rHl5hEIx2oQuLbsuEMR7CRxUMQGfMnnqEqmCki
3Dkyxl6NYxBBSqa3FPFjG7ReZ4E/j2XB4VQkcDZVCwFqBlP1sauQCww5RLsJAcvxLfUKBSNXHof9
aFqTJdUoWrScQeGihgazMqRfE7OEGw1egDsc7L50QWNuU3s16XuZBq8Kl+6FLUpjukmZlpZggPn0
HPHj5iEtvgDO9xEyZ3uTfyDsEjOvJrYUT0UlN1FHU8zTVeahYKQcXMJmOEIGvk0WMxsX6OjS9mYC
8AwA8Dk1fnRLrRzpTzFGZS74VEujE3OPpxOUXEApd4rSUOsJ2Ho6gs/YMPWwAuagiBpjewaGegE7
8KlccaF+1goVYF20RIvhxbC/Toht1FVMnG/QVvea37X0uiJ0pkM8nRJS5fBr6FT/KJxB7+xlG3aK
28qbCgmtXvLGp5VHq9/jOE5/f0P59jjf5s4XXXs/d8pXWFw8uARYW2lQ0WvZzAxDo/aySdNSazaL
1vLKzwlU04W2r1So2qpuVrer96pN9MHccG5nO9C6UmXIkVTSbKVr2WsCI9WaWxfXkiu2ecViyJK0
vrlskW3WjSdAE3tL47Rz2ICkEBUdVVGAWEFzHf0bHZJls8AC4J0Td2jpl81vntuyXg6TtBxK9tl7
GVZlNcPcERHBecsrC/u7dqJW0aMOFt8qpQrzp6OCvVcxyIzagn6d0hE3Alu5TxXQoYqYVQepU5qX
pVGwQRs5CiiTw1uZ0BsCrfXnFZccEjhu1UWmFwW03wbuljWOF5v6MJA4e7KdEBiOtxOGpbdgsttc
1OTIEBtuS6r2u6o9Vpu8axL5YW1lWB/4c59liB+B9FNIjMcgV4ZpZJZGZXbBE7G9PghO1mGaxVQ2
FdVdZQqwFhb2D5Qezt/JlChW6ikR0EgVmnHcm6KupF5nLfz67WrqdWVV9/a9lje2Gpb+50alH2hb
y01F6kAg4mW1MoTIj5b38iI841hULLHEwmpkFhbUtGi/xhwCCUFyiqjrfCblSZVqSDxJMDAr7C/m
/7L8P2szXy/7fwn/32xu3M3e/200N1s3/P/XePL8PyuI/1uy/z90UniUdW4fNjtW9U1tZMg8fi+z
ePVbZgYkrBPr70FZr/Sw5ZQ2bLotLgnl4tLN0sPX2cLpomUt2f2W6UPR3kj4ytifLwxTaeK1Gk7U
qMlDX6bjq6CpormxeoUjMEIFiwAU7iAIECSCyRNQBIY8xwFGA2zgnw0eKv7Zxj936Z9NfNhquDQ3
dQe09soPStQZ1UqkEgXIAmo6GetWnpZO82ljfx6FZ20HhkWMmRPD72oM2eiU7S5Uz6YMWHZB4AoS
XoMz0UE2PLNJUM0MhWpn/csUd8x9taCgNlboP2ijqZUBDmZWklIrp8y2nqkV/KNBxnBeedjMUcCf
V2mT6mwt51UKSEo6U6dsQWzZPTdA7cg/n0p8XdH4shGbICtT3jgYhP7E2SVw384yDRhEU3bADIzg
D3uv3xiCcvVXGaNaRkWussEFqmuAZMAV4uo7xyjNGsV11PPH9GNtrZLcHIVVdXxSVeUOro5O9o9P
9psHuBGiH60DfSmEG6WT/fJgVD4A9YvfogloX0enyW+o4R7sqMn+8cHa2o462z+u8suB1JIzfilL
M3LzxD1G4Bo1STpJFQyrqL6yw/ZOBEbIyXS9kGLtD3HfhQ4MqdtsTzV23diKeVuZcB830Gc5XMW5
6VLqFvEIsGWYwhAH0xSrW/UmbHh+mUARtkqt9A+Ac5lcI7ouiCJccLK/2Jhd15cZ1XO++/QHyQcV
03rf6ZCo7vIHJFo3hMnIqD2WDdL8/FZlB2b52ezryfRFbZQhHSYXX0vz84ffoCTL0RLVb/rjb/jI
Sqz7v2EEPlsZvuO4Dfi9Y1rC9x3+F5avqSg3fLtN2SZzj8DMxtLrqlXBvVil9OhpZ+Ud1vYtdefD
jnr7SL9+e4fqEHIVMGlDaXLl0VOtNbny9lE5kRo4uMwidR0jW0+EbMIdixN3riZjyeO8i5AZK6IW
Yh3qt8U6bx9Z5VIU+BjEdn0NXILkEl5ZxzWNdCDtkX8Yp0CaQlwF0N1RWoxqohQL4roewdbHTQ93
vkjMxSTeZQC9fJIuPCPALkO5X8XQLyIYcqQNllhx+g5L2CHrGmPVUoc1iD5yCbIBQa15xVVSOKgM
hZYh0JSxUsivTt12697nta5NCPSVTkIEpZjwgm7kYL5t5YkSW5poq4D+mcyNlYJnmxVBohVYbMOE
LCVQZHliR2mBosgTeUtsXjcqYSHi5wJu5eHrZVJEmpjLpYbpTB8pJUwXvqpUMFVquRQwk+1SqR+v
hLs5oZ/W/dboCxMJeLLAb0euN9GuyCw4lbiEaBobw3kdfUXmG8r2Id+/nHUA/IT2KkuieEnPJTt5
m43CvEguFiYKGn0PBP9eENsFEkXqYEqmqE2aqf3PFCoaaSEBmlefkqZy4p/CfGqphPGC3TFJSGBZ
GUtHDWDyJilfzxgTrlKZqsVGMb0ilg++oILlIJB1ey8jZGUGYxGHGAUbNXhJpcBQ92q9MH0kG5xW
sWxFwbHyKeLW0Wkib70irheMnmAsI2ktQvibKYS/+alCV6eXX1DqulUsdR2d5sSuo9MOJS+Xupoi
/2Zi13+aJyf/taK762sDVNQF9h8bra0C/f8b/a+v8nz7zXovnKz3/Pio9K0juFXPnz+vsaezH148
+h+ExIQxvOhfQl1dZ2sTRo2D0bAGF8zEh9J++79YRyImZuLRn6hg3AsGA0TMDecIojFej6M+2mRC
hNpNzAUIS/x978GjityhsYMzI4DOCZ1ZeQkfCMsbAXRdvSIiisPn2UrrJUb0waLEqqIrTR5TZ6Wl
/v7oVWdlQ8uVY65393ar9PrvD+jUJcaltbUNcYiwTvS1Pj/8w2qSrqpVVRs2K6VXsDTwDtnx/UmN
LXeBaI+AaXkwJSC/+/fxu5QGOAvfV1i5lZAlzgXqNfdcvX3x6se9V9vEfEY+IEOJtdNt2Dj8XKmr
N0+ePXn+N/Xmwesf20SPEaoWeJ2p3xchu4WE//iQ3QnG9dKKp9VvacQgD+BYYzVe/3X9W7W+WqGO
PAbELp2q9ETprryNQsRlZ7/+2hxknYe0XlevxVoER+/6fDyDFHY8PTG+5qhdVpQT/TpiVaiAP4Ti
DeWrUc/1zC34EuGXFW8QRrwm6XejXFmv18sE/JcvOr+svHz7aAcWKfQTKeupjuzQwqaiqjZDScpF
51E0psnTr4nC8dsOurl+ul3TBZEt4nxvy+lq3pZLgGJtolbXf+12f3zypvvD3t+ePO92f1lZr5qk
veePOGG2Kp02sG8Odn5ZGeCcBWwh/h7gTKVqzTpzb0z+9uINDcxZkZmMmRW5o89fzKGUUb+sUB1l
nLDyC5LAFVrnZTH7eJcvMQ7jMc+dB3/9CpkrqXuTFDDQGdaSAzatnf0xzHSx9tDJlgzsx45OXLcr
qyQ2Iw0xHuk0QBiyUjd6aEbWoYfGdIfIWCwLUFhrt95UDsromKYGbCFV/nnv1aMnD99QiWab/mlR
NoZEC3BgIoKhwA02UcXrNw9eMch11XGl9E4o1LFPxNQEdgMLXoq1aEdNZtG0v6NGMUz4DeVEXMuz
6SAYsf3s+18fvvzpl1+8+JdfKqtUdDYPxwHtSiCGrOFdRaeXV4CmyhVpFwQSthH1yXP7pWqKO4vA
o+XSB335xfr0ZnHTPsKdV0mzERoYq9nvsv6QonO8lj3hbgGpg0jk1TZD65eV1cJCWAk8xc0S/2mU
UjukxKjQrPxTdXc7jWO1HNTZQuUbiu2LPFn6b9JfRCfB9WoAXHz/39hqbNzN3v9vNTdu6L+v8eTv
//3BgB1PKe9Rc6vSdrSQ7VXD82oSN9a6p6pqneXAvZ1YJUYM7jTOFeyIipQBiHb6NPvMqjU2q2Zs
wooUDFztgc9XGSjSFHjaYaciuB42koBhSmBpFAWYNG62NgnWzr1Ccl9aevHq0d6rjtdQLbWpttU9
1WyoptpQW+qu+k41mxVz748j4eicJkBLZBLpLZ0woKK2mYwWR9lx2Buxd2x2KoOzUcwuJyCI4469
F/RczyxyWVZhES7lWYlX3sXtteqHlXfcyf2V8OBD2YgfNYkZ8yFsFRHyK4VI2nNQjNKrPyD+1+Ha
2Wmt54v9hNcrvBGQqw3r5CcjeV95mhbnp4T5yvEOZMUrec3v/Hpvq+cF4vSqFZkbMfpziNPqdWji
0nkfKwOmOwcfrqiNfzWpOX+Y2NK0TLZoodylpfIdL5YmZXdvSTRWdyTijvjKCtcZdlo2tTJxpMz8
wtBa8eTPpKIu1oq5+HJooxjMwjA2t43UMAGt8v5s1TfUsx9YDaWqZj7lf9pqy+3SQD3dEJE2cRfR
ecWRDH4azC1ok/1XCM/tpQDlOwkXnp5H47qjJpXKlQFboIl5wcWleyes8W2bIajDOJ/Dqw+BBqq1
6/ayGD8qWMiK42mp2i4y1OTFm0eVHUetJqut/1EwZTzIekONJl/IyjWU+cpXoL2x3ODC6z9DmyDR
kfW1w7bqqAPhbyFNZfEgMmjFKjM54myp6EYui5ozMlqp3lwlr0AWbKZihf4mM0GTifd1Whg8nVe8
D9Vy8fJScb+79raTJTdIrgDMwluqB5O+BZAk49GVVtvVrwYGRVcDtnnbiq26DbxdBdKumuUh90gF
UtoLqlHeyqDwgsCxCXJXEjTNgNn5mikPL+3Tahmw+NZJpv0zIePuvqRWq3FPjKBAo+i+pLjsUkjc
iMlvnmt7svxfvBjTQgz/COqz8+tq4xL+b2O7sZ3l/zaajRv+72s8336zvogjFkkHkxNj1lsql8uv
ZSVA8EuMIXMtq8BPjELbhECndBIyEumBUvAP4VaW9UFpNa0CtUyG4aHWQa5UtUKhSJFnTNNNh6JR
zhpAcb1UWsT+YcAaiHYNmgbVPi7Q6/X6AXQxg/OYyJqhL7ELdCAZpznB5OAwfAThJpI9HJ7TOCDq
oqEZg2VouXC4VkKyfSLnYcDMVwd9IkJEFzpWOu+LaAAd6keEP0ulN0+e7b2GtXKBJmdVlROX53hb
5vAc33LuzpGobYCphOsXpJxQzPwlV/GCHVrSH6cq/Q3WPkmjxoF5QeNOcwVuzlHYcYdiX5lzTz6C
C7dv4g6FHXQm7lDsqy7pNJv4ReGuBv24XCk9ecTAFkef4rgzccNpfGtq17emo9b7qvFjip+wAsJf
mELhL5tBMaColRLc//H1jTfz50cVsU2Ppqfwpbp/YLWJ2XsJ0R9TYgfdnHhoH3C8GKiLcmgP0d4t
K3YDa7MdI6IcC6mjoD4k1hLhyqOy98vpWqXj/fL6DjvdpGoqbpn9MlVfZn1T/zDRN6UOmnCIxycV
13shPsmw4AT2TPdzHp1bt5BD4hPm9EV8PJ3BLkB5b85nwV4UTaOq4lhg/Ltiyzxnqha1MhKIPTRT
pX0Wd5480m0g8IKzZbyKhR4bD6BEAjTtvDPS46vCN+d8gbBuUHuOoN/MRVknmpqBfi7/jhLoHCIq
s8YJ8E9ZpRmrGLBEKagcSuex870orgqy0P3GvOwDWBH7bpVPFafj8UGS7yxxUXCCPp3hVhJqjYDQ
gXG6ioSTFPCq+t8GZxlz7BiDheqCJ730RI6Jsh77Z5SqakQ14zM7pSeOEtQvtTLWvgrgwX4UcPkS
JXe7uH7odlkzu9vl2DHdcmply4qHEwR4vEbAQZkrQaEdZb+02gfwjFCkvp5Dennckuwfnp2Io1o4
C6hSD+fBOPaczURo2Nl6pjibT0rvkqwMSMLgsyo7nEjmNhc/euxOUpvbMMtkWH43/NB5N27XNw8/
KO//+3/fxbN2vTn8cEtNOu8mHyrlpDZxMkFD2W8csF6+BNahKo6pipMPZRlqFStjgiHvN9sH8Bf8
vuxkR+u0xf9qKuDmuXlunpvn5rl5bp6b5+a5eW6em+fmuXlunpvn5rl5bp6b5+a5eW6em+fmuXlu
nn+F5/8HHZ2SOwDgAQA=
__KIT_END__
