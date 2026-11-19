#!/usr/bin/env python3
"""Oracle for persistence.sqlite._state.guard.py pure predicate logic:
_running_under_pytest (env truthiness), _process_looks_like_pytest (argv
analysis over a fake proc.cmdline()) and _is_production_state_db (Path
predicate). The harness (t_port_state_guard.c) emits one JSON line per case.

The env-based predicate is driven by temporarily setting the three env vars
inside THIS process (saved/restored per case), so the LIVE function reads
exactly the values the C side was fed.
"""
import json
import os
import sys
from pathlib import Path

for _quarry in ("C:/Users/eman5/hermes-agent", os.path.expanduser("~/hermes-agent")):
    if os.path.isdir(os.path.join(_quarry, "persistence")):
        sys.path.insert(0, _quarry)
        break

try:
    from persistence.sqlite._state.guard import (                       # noqa: E402
        _is_production_state_db, _process_looks_like_pytest,
        _running_under_pytest,
    )
except ImportError:
    # The suite interpreter is the quarry venv; re-exec under it when the
    # current interpreter lacks the quarry import chain (stdin survives execv).
    _venv = "C:/Users/eman5/hermes-agent/.venv/Scripts/python.exe"
    if not os.path.isfile(_venv):
        _venv = os.path.expanduser("~/hermes-agent/.venv/bin/python")
    if os.path.isfile(_venv) and \
            os.path.abspath(sys.executable) != os.path.abspath(_venv):
        os.execv(_venv, [_venv, os.path.abspath(__file__)] + sys.argv[1:])
    raise

sys.stdin.reconfigure(encoding="utf-8")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

_ENV_NAMES = ("PYTEST_CURRENT_TEST", "PYTEST_VERSION", "HERMES_TEST_ISOLATION")


class FakeProc:
    """Duck-typed psutil.Process: only cmdline() is consulted."""

    def __init__(self, args, unreadable=False):
        self._args = args
        self._unreadable = unreadable

    def cmdline(self):
        if self._unreadable:
            raise RuntimeError("cmdline unreadable")
        return self._args


def env_verdict(i):
    saved = {k: os.environ.get(k) for k in _ENV_NAMES}
    try:
        for k, v in zip(_ENV_NAMES, (i.get("pytest_current_test"),
                                     i.get("pytest_version"),
                                     i.get("marker"))):
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
        return bool(_running_under_pytest())
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def main() -> int:
    total = mism = 0
    for line in sys.stdin.read().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            c = json.loads(line)
        except ValueError:
            continue
        total += 1
        fn = c.get("fn")
        i = c.get("in") or {}
        try:
            if fn == "_running_under_pytest":
                want = env_verdict(i)
            elif fn == "_process_looks_like_pytest":
                if i.get("cmdline_error"):
                    proc = FakeProc(None, unreadable=True)
                else:
                    proc = FakeProc(i.get("cmdline") or [])
                want = bool(_process_looks_like_pytest(proc))
            elif fn == "_is_production_state_db":
                want = bool(_is_production_state_db(Path(i["resolved"]),
                                                    Path(i["root"])))
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"MISMATCH {c.get('case')} ({fn}): python raised {exc!r}")
            mism += 1
            continue
        got = c["out"]
        if got != want:
            mism += 1
            print(f"MISMATCH {c.get('case')} ({fn}): C={str(c['out'])[:200]} "
                  f"PY={want}")
    if total == 0:
        print("ORACLE state_guard: 0 cases (link failure in disguise)")
        return 1
    print(f"ORACLE state_guard: {total} cases, {mism} mismatches")
    return 1 if mism else 0


if __name__ == "__main__":
    sys.exit(main())
