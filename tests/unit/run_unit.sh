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

echo "== test_setup_beat (the onboarding tool end to end, on a real state.db)"
SB_OBJS="src/tools/setup_beat_tool.o src/agent/onboarding_flow_apply.o src/agent/onboarding_reminders.o \
    src/agent/onboarding_task_catalog.o src/agent/onboarding_task_catalog_data.o src/agent/onboarding_text.o \
    src/cli/paths.o src/tools/registry.o src/persistence/sqlite/_state/hermes_state_open.o \
    src/persistence/sqlite/_state/hermes_state_lifecycle.o lib/libdb/sqlite3.o lib/libjson/json.o"
if [ -f src/tools/setup_beat_tool.o ]; then
    if $CCBIN -O1 -w -I include -I lib -I lib/libdb -include hermes_win32.h -D_GNU_SOURCE -include stdlib.h \
            tests/unit/test_setup_beat.c $SB_OBJS -o "$OUT/t_sb.exe" 2>"$OUT/t_sb.build.log"; then
        SB_TEST_HOME="$OUT/sb_home" "$OUT/t_sb.exe" || fail=1
    else
        echo "   BUILD FAIL"; sed -n '1,6p' "$OUT/t_sb.build.log"; fail=1
    fi
else
    echo "   SKIPPED (src/tools/setup_beat_tool.o not built)"
fi

echo "== connectors_validation_oracle (differential: connectors validation vs tool.py)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/connectors_validation_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== vault_store_oracle (differential: vault helpers vs agent.vault_store)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/vault_store_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== login_classifier_oracle (differential: login control classifier vs vault_login_classifier)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/login_classifier_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== fill_js_oracle (differential: fill script builder vs build_fill_js)"
if command -v "$PYBIN" >/dev/null 2>&1; then
    if PATH="/c/msys64/mingw64/bin:$PATH" "$PYBIN" tests/oracle/fill_js_oracle.py; then
        :
    else
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== fernet_oracle (spec vectors + interop with cryptography)"
FERNET_PY="${FERNET_PY:-$PYBIN}"
if command -v "$FERNET_PY" >/dev/null 2>&1; then
    PATH="/c/msys64/mingw64/bin:$PATH" "$FERNET_PY" tests/oracle/fernet_oracle.py
    rc=$?
    if [ $rc -eq 3 ]; then
        echo "   ORACLE SKIPPED (cryptography not importable -- set FERNET_PY)"
    elif [ $rc -ne 0 ]; then
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo "== vault_file_oracle (interop: C reads the vault Python writes, and back)"
if command -v "$FERNET_PY" >/dev/null 2>&1; then
    PATH="/c/msys64/mingw64/bin:$PATH" PYTHONPATH="${FERNET_PYTHONPATH:-}" "$FERNET_PY" tests/oracle/vault_file_oracle.py
    rc=$?
    if [ $rc -eq 3 ]; then
        echo "   ORACLE SKIPPED (needs cryptography + USERPROFILE/HOME)"
    elif [ $rc -ne 0 ]; then
        echo "   ORACLE FAIL"; fail=1
    fi
else
    echo "   SKIPPED (no python on PATH)"
fi

echo
[ $fail -eq 0 ] && echo "unit tests: PASS" || echo "unit tests: FAIL"
exit $fail
