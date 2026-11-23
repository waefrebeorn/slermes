#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_updates.py (gate w13_pluginsupd_a7336f)."""
import importlib.util
import json
import sys
import types

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
cli = types.ModuleType("hermes_cli")
cli.__path__ = [QUARRY + "/hermes_cli"]
sys.modules.setdefault("hermes_cli", cli)


def _stub(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


class ProvenanceClass:
    MANUAL = "manual"
    DRIFT = "drift"
    SELF_CLONED = "self_cloned"


_stub("hermes_cli.plugins_provenance", Provenance=dict,
      ProvenanceClass=ProvenanceClass, plugins_provenance=lambda d: [])

SRC = QUARRY + "/hermes_cli/plugins_updates.py"
NAME = "plugins_updates_live_w13_pluginsupd"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "https_update_url":
        try:
            return {"ok": True, "value": live.https_update_url(r.get("url"))}
        except ValueError as exc:
            return {"ok": False, "error": str(exc)}
    if func == "version_is_newer":
        return live._version_is_newer(r.get("latest", ""), r.get("current", ""))
    if func == "check_result_json":
        return live.CheckResult(
            name=r.get("name", ""), klass=r.get("klass", ""),
            current=r.get("current"), latest=r.get("latest"),
            update_available=r.get("update_available"),
            needs_fixing=r.get("needs_fixing"), min_hermes=r.get("min_hermes"),
            reason=r.get("reason", ""),
        ).to_json()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    if r.get("func") is None:
        continue
    cases += 1
    py = compute(r)
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (r.get("func"), r.get("case"), py, got), file=sys.stderr)
print("ORACLE w13_pluginsupd_a7336f: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
