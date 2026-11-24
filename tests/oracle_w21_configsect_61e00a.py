#!/usr/bin/env python3
"""Differential oracle for hermes_cli/config_section_guard.py (gate w21_configsect_61e00a).

The live `_guard_section_overwrite` is executed as-is. Its only dependency is
the CLI sink `hermes_cli.config._exit_invalid` (config seam: print to stderr,
SystemExit(1)), which is stubbed in sys.modules with that exact behaviour so the
oracle can capture the exit instead of dying. stdout/stderr are captured, so the
compared value is (returned key, both streams, exit code) -- everything the
function does.
"""
import contextlib
import importlib.util
import io
import json
import sys
import types
from pathlib import Path

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")

REPO = Path(__file__).resolve().parent.parent
QUARRY = REPO.parent / "hermes-agent"
SRC = QUARRY / "hermes_cli" / "config_section_guard.py"
GATE = "w21_configsect_61e00a"

if str(QUARRY) not in sys.path:
    sys.path.insert(0, str(QUARRY))

# config seam: the live sink is `print(msg, file=sys.stderr); sys.exit(1)`.
_sink = types.ModuleType("hermes_cli.config")


def _exit_invalid(msg: str) -> None:
    print(msg, file=sys.stderr)
    raise SystemExit(1)


_sink._exit_invalid = _exit_invalid
sys.modules["hermes_cli.config"] = _sink

_spec = importlib.util.spec_from_file_location("config_section_guard_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules["config_section_guard_live"] = live
_spec.loader.exec_module(live)


def expected(r):
    out, err = io.StringIO(), io.StringIO()
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            key = live._guard_section_overwrite(r.get("key"), r.get("value"),
                                                r.get("user_config") or {}, bool(r.get("force")))
        return {"status": "ok", "key": key, "stdout": out.getvalue(), "stderr": err.getvalue()}
    except SystemExit as exc:
        return {"status": "exit", "code": exc.code, "stdout": out.getvalue(), "stderr": err.getvalue()}


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if not isinstance(r, dict) or r.get("func") != "guard":
            continue
        cases += 1
        try:
            py = expected(r)
        except Exception as exc:  # noqa: BLE001
            mism += 1
            print("MISMATCH %s case=%r live-error=%r" % (r.get("func"), r.get("case"), exc),
                  file=sys.stderr)
            continue
        got = r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s case=%r\n  py=%r\n   c=%r"
                  % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
    print("ORACLE %s: %d cases, %d mismatches" % (GATE, cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
