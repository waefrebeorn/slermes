#!/usr/bin/env python3
"""Differential oracle for gate w14_redactsecr_02f95a (agent/redact_secret_sources.py)."""
import importlib.util
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

# hermes_home_key() is an injected seam; the applied-value registry is per-fixture.
hc = types.ModuleType("hermes_constants")
hc.hermes_home_key = lambda *a, **k: "home"
sys.modules["hermes_constants"] = hc

NAME = "w14_redactsecr_02f95a_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "redact_secret_sources.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def compute(r):
    f = r.get("func")
    if f == "pem_texts":
        return live._pem_texts(r.get("value", ""))
    if f == "multiline_fragments":
        return live._multiline_fragments(r.get("value", ""))
    if f == "redact_exact":
        live._VALUES_BY_HOME = {"home": tuple(r.get("values") or [])}
        return live.redact_secret_source_values(r.get("text", ""))
    return None


def main():
    cases = mism = 0
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        if not isinstance(r, dict) or "out" not in r:
            continue
        cases += 1
        py, got = compute(r), r.get("out")
        if py != got:
            mism += 1
            print("MISMATCH %s py=%r c=%r" % (r.get("func"), py, got), file=sys.stderr)
    print("ORACLE w14_redactsecr_02f95a: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
