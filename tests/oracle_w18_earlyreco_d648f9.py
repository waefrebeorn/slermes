#!/usr/bin/env python3
"""Differential oracle for hermes_cli/_early_recovery_zip.py (w18_earlyreco_d648f9)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mk(name, **attrs):
    m = types.ModuleType(name)
    m.__path__ = []
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


if "hermes_cli" not in sys.modules:
    _mk("hermes_cli")
_mk("hermes_cli._early_recovery", _CLAIM_HELD_ERRNOS=set(), ZIP_SWAP_JOURNAL=".journal",
    _keep_aside=lambda p: p, _lock_fd=lambda *a: None, _project_root=lambda: None,
    _pytest_owns_live_checkout=lambda r: False, write_durable_text=lambda *a: None)

SRC = Q + "/hermes_cli/_early_recovery_zip.py"
spec = importlib.util.spec_from_file_location("w18_earlyreco_d648f9_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
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
    if r.get("func") != "parse_journal":
        continue
    try:
        parsed = live._parse_zip_swap_journal(r.get("raw"))
    except Exception as exc:
        print(f"LIVE ERROR parse_journal {r}: {exc!r}", file=sys.stderr)
        continue
    if parsed is None:
        py = None
    else:
        phase, gen, entries, temps = parsed
        py = {"phase": phase, "gen": gen,
              "entries": [[n, b, s, l] for (n, b, s, l) in entries],
              "temps": temps}
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH parse_journal args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w18_earlyreco_d648f9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
