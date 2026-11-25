#!/usr/bin/env bash
# Build and run the pure unit tests under tests/unit/.
#
# Each test links the real object the binary ships (not a copy of the code) and stubs only the
# layer it does not need (drawing for the controller, nothing for the manifest reader). Exits
# non-zero on any failed assertion.
set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO"
OUT="$REPO/.testbin"
mkdir -p "$OUT"

INC="-I include -I src -I C:/msys64/mingw64/include/SDL2 -I C:/msys64/mingw64/include -DSDL_MAIN_HANDLED"
CCBIN="${CC:-gcc}"
fail=0

echo "== test_desktop_controller"
if $CCBIN -O0 $INC -c src/desktop_controller.c -o "$OUT/desktop_controller.o" 2>"$OUT/dc.build.log"; then
    if $CCBIN -O0 $INC tests/unit/test_desktop_controller.c "$OUT/desktop_controller.o" \
            -o "$OUT/t_dc.exe" 2>"$OUT/t_dc.build.log"; then
        "$OUT/t_dc.exe" || fail=1
    else
        echo "   BUILD FAIL"; sed -n '1,6p' "$OUT/t_dc.build.log"; fail=1
    fi
else
    echo "   COMPILE FAIL"; sed -n '1,6p' "$OUT/dc.build.log"; fail=1
fi

echo "== test_entrypoint_manifests"
if [ -f src/port_missing_impl.o ]; then
    if $CCBIN -O0 $INC tests/unit/test_entrypoint_manifests.c src/port_missing_impl.o \
            lib/libjson/json.o -o "$OUT/t_ep.exe" 2>"$OUT/t_ep.build.log"; then
        PYTHONPATH="$REPO/tests/unit/fixtures/ep_fixture" "$OUT/t_ep.exe" || fail=1
    else
        echo "   BUILD FAIL"; sed -n '1,6p' "$OUT/t_ep.build.log"; fail=1
    fi
else
    echo "   SKIPPED (src/port_missing_impl.o not built)"
fi

echo "== onboarding_flow_oracle (differential: C port vs session_rules.onboarding_flow)"
# The port's contract is "same answer as Python", so the check is a real differential run over the
# quarry flow, not a hand-written expectation table.
PYBIN="${PYTHON:-python}"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/onboarding_flow_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== onboarding_reminders_oracle (differential: C reminder text vs agent.onboarding_text)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/onboarding_reminders_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== onboarding_catalog_oracle (differential: generated C table vs onboarding_first_tasks)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/onboarding_catalog_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo
[ $fail -eq 0 ] && echo "unit tests: PASS" || echo "unit tests: FAIL"
exit $fail
