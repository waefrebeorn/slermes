#!/usr/bin/env bash
# The one entry point. Everything a change has to pass, in the order it can fail cheaply.
#
#   bash scripts/check.sh            # fast: artifact runs, build healthy, numbers derived
#   bash scripts/check.sh --gates    # + held-out fuzz sweep over every wave plan
#   bash scripts/check.sh --full     # + rebuild and re-derive everything from scratch
#
# Rules this encodes, learned the hard way:
#   * A silent exit is a failure. slermes.exe prints NOTHING and exits 127 when its DLLs
#     are missing, and every script downstream reads that as "not run". The first check
#     here is that the binary speaks.
#   * Numbers come from the tree on THIS run. Nothing is carried between runs.
#   * Never pipe a gate through head/tail: it corrupts exit codes.
set -u
cd "$(dirname "$0")/.." || exit 1

PY=${PY:-C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe}
MODE=${1:-}
FAIL=0
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
ok()   { printf '   ok    %s\n' "$*"; }
bad()  { printf '   FAIL  %s\n' "$*"; FAIL=1; }

if [ "$MODE" = "--full" ]; then
  step "build"
  # shellcheck disable=SC1091
  source build/winlibs-env.sh
  make -j8 slermes USERPROFILE="C:\\Users\\eman5" \
       LOCALAPPDATA="C:\\Users\\eman5\\AppData\\Local" > .testbin/check-link.log 2>&1
  rc=$?
  [ $rc -eq 0 ] && ok "link rc=0" || bad "link rc=$rc (see .testbin/check-link.log)"
fi

step "artifact"
bash scripts/bundle_dlls.sh >/dev/null 2>&1 || bad "bundle_dlls"
out=$(./slermes.exe --version 2>&1); rc=$?
[ "$rc" = 0 ] && [ -n "$out" ] && ok "version rc=0" || bad "version rc=$rc out=${out:-<empty>}"
for c in --help doctor completions; do
  n=$(./slermes.exe "$c" 2>&1 | wc -c)
  [ "$n" -gt 40 ] && ok "$c speaks ($n B)" || bad "$c silent ($n B)"
done

step "build health"
$PY tests/verify_build_health.py --no-link > .testbin/check-health.log 2>&1 \
  && ok "health clean" || bad "health (see .testbin/check-health.log)"

step "surface (derived now)"
$PY scripts/port_audit.py | sed -n '1,14p'
echo
$PY scripts/classify_surface.py | sed -n '1,9p'

if [ "$MODE" = "--gates" ] || [ "$MODE" = "--full" ]; then
  step "held-out gates"
  for w in 25 26 27 28 29 30 31 32 33 34 35; do
    plan=docs/wave${w}-plan.json
    [ -f "$plan" ] || continue
    out=$($PY -u scripts/gate_fuzz.py --plan "$plan" --per-case 5 --jobs 4 2>&1)
    printf '%s\n' "$out" | grep -q '^BAD' \
      && printf '   wave %s: %s carrying\n' "$w" "$(printf '%s\n' "$out" | grep -c '^BAD')" \
      || printf '   wave %s: clean\n' "$w"
  done
fi

step "result"
if [ $FAIL -eq 0 ]; then echo "   PASS"; else echo "   FAIL"; fi
exit $FAIL
