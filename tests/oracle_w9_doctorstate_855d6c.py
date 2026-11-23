#!/usr/bin/env python3
"""Differential oracle for hermes_cli/doctor_state.py (w9_doctorstate_855d6c gate).

The live module's pure formatting core _render_state_db_stats is exercised
directly; doctor's I/O dependencies and the sizefmt import are stubbed/loaded
from the quarry so the real helper semantics are compared.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


def _pkg(name):
    m = _mod(name)
    m.__path__ = []
    return m


_pkg("hermes_cli")


def _doctor_check(*args, **kwargs):
    def deco(fn):
        return fn
    return deco


class _NoopCM:
    def __init__(self, *a, **k):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *a):
        return False


_mod("hermes_cli.doctor_report",
     Finding=type("Finding", (), {}),
     _fail_and_issue=lambda *a, **k: None,
     _section=lambda *a, **k: None,
     check_bool=lambda *a, **k: True,
     check_info=lambda *a, **k: None,
     check_ok=lambda *a, **k: None,
     check_warn=lambda *a, **k: None,
     doctor_check=_doctor_check,
     ensure_dir=lambda *a, **k: None,
     warn_on_error=_NoopCM)
_mod("persistence_assembly", open_store_admin=lambda *a, **k: None)

# Load the REAL sizefmt helper from the quarry, register it as the module the
# live module imports.
sspec = importlib.util.spec_from_file_location("w9ds_sizefmt", Q + "/hermes_cli/sizefmt.py")
sizefmt = importlib.util.module_from_spec(sspec)
sys.modules["w9ds_sizefmt"] = sizefmt
sspec.loader.exec_module(sizefmt)
sys.modules["hermes_cli.sizefmt"] = sizefmt

SRC = Q + "/hermes_cli/doctor_state.py"
spec = importlib.util.spec_from_file_location("w9ds_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w9ds_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "format_bytes":
            py = live._human_bytes(r.get("value"))
        elif func == "render":
            py = [list(row) for row in live._render_state_db_stats(
                r.get("stats"), r.get("holders"), r.get("host_note", ""))]
        elif func == "malformed":
            py = any(m in str(r.get("exc", "")).lower() for m in live._MALFORMED_DB_MARKERS)
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w9_doctorstate_855d6c: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
