#!/usr/bin/env python3
"""Differential oracle for gate w14_primaryaut_c69abc (agent/primary_auth_fallback.py)."""
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


def _pkg(name):
    m = sys.modules.get(name)
    if m is None:
        m = types.ModuleType(name)
        m.__path__ = [os.path.join(Q, *name.split("."))]
        sys.modules[name] = m
    return m


_pkg("agent")
# Deterministic seams for the two in-function imports of primary_auth_lead.
rd = types.ModuleType("agent.redact")
rd.redact_sensitive_text = lambda text, force=False: text
sys.modules["agent.redact"] = rd
tfc = types.ModuleType("agent.turn_failure_copy")
tfc.provider_label_for = lambda p: "provider:" + p
tfc.short_detail = lambda e, n: str(e)[:n]
tfc.relogin_command_hint = lambda slug: "hermes auth login " + slug
sys.modules["agent.turn_failure_copy"] = tfc

NAME = "w14_primaryaut_c69abc_live"
sp = importlib.util.spec_from_file_location(NAME, os.path.join(Q, "agent", "primary_auth_fallback.py"))
live = importlib.util.module_from_spec(sp)
sys.modules[NAME] = live
sp.loader.exec_module(live)


def _failure(d):
    if not isinstance(d, dict):
        return None
    return live.PrimaryAuthFailure(provider=d.get("provider", ""), error=d.get("error", ""),
                                   relogin=d.get("relogin", ""), backup=d.get("backup", ""))


def _asdict(f):
    return {"provider": f.provider, "error": f.error, "relogin": f.relogin, "backup": f.backup}


def compute(r):
    f = r.get("func")
    if f == "route_snapshot":
        return list(live.route_snapshot(r.get("runtime_kwargs")))
    if f == "failure_for_route":
        out = live.failure_for_route(_failure(r.get("failure")),
                                     tuple(r.get("resolved") or ()), r.get("runtime_kwargs"))
        return _asdict(out) if out is not None else None
    if f == "primary_auth_lead":
        return live.primary_auth_lead(_failure(r.get("failure")), backup=r.get("backup", ""),
                                      failure_reason=r.get("failure_reason"), error=r.get("error"))
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
    print("ORACLE w14_primaryaut_c69abc: %d cases, %d mismatches" % (cases, mism))
    sys.exit(1 if mism else 0)


if __name__ == "__main__":
    main()
