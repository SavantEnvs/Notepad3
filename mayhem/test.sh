#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer self test built by mayhem/build.sh
# (mayhem/build/np3_selftest: TinyExpr++ results, uchardet charset verdicts, PCRE2 find/replace
# positions and substitutions through Scintilla's Document, Lexilla CPP styles).
# Notepad3 ships no Linux-runnable test suite, so this asserts exact outputs of its engines.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN="$SRC/mayhem/build/np3_selftest"
EXPECTED=19   # number of checks in mayhem/harnesses/np3_selftest.cxx
if [ ! -x "$BIN" ]; then
  echo "test.sh: $BIN missing — run mayhem/build.sh first" >&2
  emit_ctrf np3-selftest 0 "$EXPECTED"
  exit 1
fi
out="$("$BIN" 2>&1)"; rc=$?
echo "$out"
passed=$(grep -c '^PASS ' <<<"$out" || true)
failed=$(grep -c '^FAIL ' <<<"$out" || true)
# Checks that never reported (crash, early exit) count as failures.
missing=$(( EXPECTED - passed - failed ))
[ "$missing" -gt 0 ] && failed=$(( failed + missing ))
[ "$rc" -ne 0 ] && [ "$failed" -eq 0 ] && failed=1
emit_ctrf np3-selftest "$passed" "$failed"
