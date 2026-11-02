#!/bin/bash
# Task 096b -- (re-run of 096 with --features host, as upstream CI: .github/workflows/rust.yml:955-960; 096 ran 0 tests) meshsim WORKER18 (W5b): capture upstream Jolt's inline test vectors. Upstream's own inline tests (jolt
# 17a9459, the crates jolt-inlines-{secp256k1,sha2,keccak256,blake2,blake3,bigint}) run with a 37-line patch to
# tracer/src/utils/inline_test_harness.rs (embedded below, SHA-256 checked): with JOLT_INLINE_VECTORS set,
# InlineTestHarness::execute_inline appends each call's operand registers and memory span before ("pre") and after
# ("post") as JSON lines; nothing else changes, and the tests still assert every output against their independent
# references (arkworks, the crates' exec functions, published test vectors). One test thread, so lines stay in order.
#   results: vectors.jsonl.xz, tests.log (cargo test output), summary.txt
# Expected 10-25 min (a cold debug build of the six crates and their tests).
set -u
cd "$(dirname "$0")/.."
RES=$PWD/results/096b-w18-inline-vectors; mkdir -p "$RES"
W=/tmp/w18-096b; rm -rf "$W"; mkdir -p "$W"
WANT=f8824f1f16336c759d665bd4c911c2c85dbfad62dc8cbb79456f6b48ca3c5451
sed -n '/^__KIT_BEGIN__$/,/^__KIT_END__$/p' "$0" | sed '1d;$d' | base64 -d > "$W/harness.patch" || exit 2
GOT=$(sha256sum "$W/harness.patch" | cut -d' ' -f1); echo "patch sha256 $GOT"
[ "$GOT" = "$WANT" ] || { echo "patch sha256 mismatch (want $WANT)"; exit 2; }
echo "start $(date -u) on $(hostname), $(nproc) threads"
if ! command -v cargo >/dev/null 2>&1; then
  export RUSTUP_HOME=$W/rustup CARGO_HOME=$W/cargo
  curl -sSf https://sh.rustup.rs -o $W/rustup-init.sh && sh $W/rustup-init.sh -y --no-modify-path --profile minimal >/dev/null || { echo "rustup failed"; exit 1; }
  export PATH=$CARGO_HOME/bin:$PATH
fi
git clone -q --filter=blob:none https://github.com/a16z/jolt.git $W/jolt && cd $W/jolt && git checkout -q 17a9459082ec62fe8b50d61cc1f38cc060bd20f8 || { echo "jolt clone failed"; exit 1; }
git apply $W/harness.patch || { echo "patch failed"; exit 1; }
git diff --stat
export JOLT_INLINE_VECTORS=$W/vectors.jsonl
RC=0; for pkg in jolt-inlines-secp256k1 jolt-inlines-sha2 jolt-inlines-keccak256 jolt-inlines-blake2 jolt-inlines-blake3 jolt-inlines-bigint; do echo "== $pkg" >> $RES/tests.log; cargo test --features host -p $pkg -- --test-threads=1 >> $RES/tests.log 2>&1 || RC=1; done
echo "cargo test exit $RC at $(date -u)"
grep -E "^test result|FAILED|panicked" $RES/tests.log | head -40
{ echo "cargo test exit $RC"; grep -E "^test result" $RES/tests.log; echo "vector lines: $(wc -l < $W/vectors.jsonl) (pre $(grep -c '"kind":"pre"' $W/vectors.jsonl), post $(grep -c '"kind":"post"' $W/vectors.jsonl))"; } > $RES/summary.txt
cat $RES/summary.txt
xz -9 -c $W/vectors.jsonl > $RES/vectors.jsonl.xz; ls -la $RES/vectors.jsonl.xz
rm -rf "$W"
exit $RC
__KIT_BEGIN__
ZGlmZiAtLWdpdCBhL3RyYWNlci9zcmMvdXRpbHMvaW5saW5lX3Rlc3RfaGFybmVzcy5ycyBiL3Ry
YWNlci9zcmMvdXRpbHMvaW5saW5lX3Rlc3RfaGFybmVzcy5ycwppbmRleCA3YzU3NDI4MzMuLjE5
NTc3ZTNlNiAxMDA2NDQKLS0tIGEvdHJhY2VyL3NyYy91dGlscy9pbmxpbmVfdGVzdF9oYXJuZXNz
LnJzCisrKyBiL3RyYWNlci9zcmMvdXRpbHMvaW5saW5lX3Rlc3RfaGFybmVzcy5ycwpAQCAtMjI5
LDcgKzIyOSw0MSBAQCBpbXBsIElubGluZVRlc3RIYXJuZXNzIHsKICAgICB9CiAKICAgICBwdWIg
Zm4gZXhlY3V0ZV9pbmxpbmUoJm11dCBzZWxmLCBpbnN0cnVjdGlvbjogSU5MSU5FKSB7CisgICAg
ICAgIC8vIG1lc2hzaW0gV09SS0VSMTggKFc1Yik6IHdpdGggSk9MVF9JTkxJTkVfVkVDVE9SUz08
ZmlsZT4sIGFwcGVuZCB0aGlzIGNhbGwgYXMgdGVzdCB2ZWN0b3JzOiB0aGUgb3BlcmFuZAorICAg
ICAgICAvLyByZWdpc3RlcnMgYW5kIHRoZSBtZW1vcnkgc3BhbiBiZWZvcmUgKCJwcmUiKSBhbmQg
YWZ0ZXIgKCJwb3N0Iik7IGEgY2FsbCB0aGF0IHBhbmljcyBsZWF2ZXMgb25seSAicHJlIi4KKyAg
ICAgICAgbGV0IGxvZyA9IHN0ZDo6ZW52Ojp2YXIoIkpPTFRfSU5MSU5FX1ZFQ1RPUlMiKS5vaygp
OworICAgICAgICBpZiBsZXQgU29tZShwYXRoKSA9ICZsb2cgeworICAgICAgICAgICAgc2VsZi53
MThfbG9nKHBhdGgsICJwcmUiLCAmaW5zdHJ1Y3Rpb24pOworICAgICAgICB9CiAgICAgICAgIGlu
c3RydWN0aW9uLnRyYWNlKCZtdXQgc2VsZi5jcHUsIE5vbmUpOworICAgICAgICBpZiBsZXQgU29t
ZShwYXRoKSA9ICZsb2cgeworICAgICAgICAgICAgc2VsZi53MThfbG9nKHBhdGgsICJwb3N0Iiwg
Jmluc3RydWN0aW9uKTsKKyAgICAgICAgfQorICAgIH0KKworICAgIGZuIHcxOF9sb2coJm11dCBz
ZWxmLCBwYXRoOiAmc3RyLCBraW5kOiAmc3RyLCBpbnN0cnVjdGlvbjogJklOTElORSkgeworICAg
ICAgICB1c2Ugc3RkOjppbzo6V3JpdGU7CisgICAgICAgIGxldCBtdXQgZW5kID0gc2VsZi5sYXlv
dXQub3V0cHV0X2Jhc2UgKyBzZWxmLmxheW91dC5vdXRwdXRfc2l6ZSBhcyB1NjQ7CisgICAgICAg
IGVuZCA9IGVuZC5tYXgoc2VsZi5sYXlvdXQuaW5wdXRfYmFzZSArIHNlbGYubGF5b3V0LmlucHV0
X3NpemUgYXMgdTY0KTsKKyAgICAgICAgaWYgbGV0IChTb21lKGIpLCBTb21lKG4pKSA9IChzZWxm
LmxheW91dC5pbnB1dDJfYmFzZSwgc2VsZi5sYXlvdXQuaW5wdXQyX3NpemUpIHsKKyAgICAgICAg
ICAgIGVuZCA9IGVuZC5tYXgoYiArIG4gYXMgdTY0KTsKKyAgICAgICAgfQorICAgICAgICBsZXQg
bXV0IGhleCA9IFN0cmluZzo6bmV3KCk7CisgICAgICAgIGxldCBtdXQgYSA9IERSQU1fQkFTRTsK
KyAgICAgICAgd2hpbGUgYSA8IGVuZCB7CisgICAgICAgICAgICBsZXQgdyA9IHNlbGYuY3B1Lm1t
dS5sb2FkX2RvdWJsZXdvcmQoYSkuZXhwZWN0KCJ3MTg6IGxvYWQiKS4wOworICAgICAgICAgICAg
aGV4LnB1c2hfc3RyKCZmb3JtYXQhKCJ7dzowMTZ4fSIpKTsKKyAgICAgICAgICAgIGEgKz0gODsK
KyAgICAgICAgfQorICAgICAgICBsZXQgbyA9IGluc3RydWN0aW9uLm9wZXJhbmRzOworICAgICAg
ICBsZXQgbGluZSA9IGZvcm1hdCEoCisgICAgICAgICAgICAie3tcImtpbmRcIjpcIntraW5kfVwi
LFwidGVzdFwiOlwie31cIixcIm9wY29kZVwiOnt9LFwiZnVuY3QzXCI6e30sXCJmdW5jdDdcIjp7
fSxcInJzMVwiOnt9LFwicnMyXCI6e30sXCJyczNcIjp7fSxcInhfcnMxXCI6e30sXCJ4X3JzMlwi
Ont9LFwieF9yczNcIjp7fSxcImJhc2VcIjp7fSxcIndvcmRzXCI6XCJ7aGV4fVwifX1cbiIsCisg
ICAgICAgICAgICBzdGQ6OnRocmVhZDo6Y3VycmVudCgpLm5hbWUoKS51bndyYXBfb3IoIj8iKSwg
aW5zdHJ1Y3Rpb24ub3Bjb2RlLCBpbnN0cnVjdGlvbi5mdW5jdDMsIGluc3RydWN0aW9uLmZ1bmN0
NywKKyAgICAgICAgICAgIG8ucnMxLCBvLnJzMiwgby5yczMsIHNlbGYuY3B1Lnhbby5yczEgYXMg
dXNpemVdIGFzIHU2NCwgc2VsZi5jcHUueFtvLnJzMiBhcyB1c2l6ZV0gYXMgdTY0LAorICAgICAg
ICAgICAgc2VsZi5jcHUueFtvLnJzMyBhcyB1c2l6ZV0gYXMgdTY0LCBEUkFNX0JBU0UsCisgICAg
ICAgICk7CisgICAgICAgIGxldCBtdXQgZiA9IHN0ZDo6ZnM6Ok9wZW5PcHRpb25zOjpuZXcoKS5j
cmVhdGUodHJ1ZSkuYXBwZW5kKHRydWUpLm9wZW4ocGF0aCkuZXhwZWN0KCJ3MTg6IG9wZW4iKTsK
KyAgICAgICAgZi53cml0ZV9hbGwobGluZS5hc19ieXRlcygpKS5leHBlY3QoIncxODogd3JpdGUi
KTsKICAgICB9CiAKICAgICBwdWIgZm4gZXhlY3V0ZV9zZXF1ZW5jZSgmbXV0IHNlbGYsIHNlcXVl
bmNlOiAmW0luc3RydWN0aW9uXSkgewo=
__KIT_END__
