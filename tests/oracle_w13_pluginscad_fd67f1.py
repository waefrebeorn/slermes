#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_cadence.py (gate w13_pluginscad_fd67f1)."""
import importlib.util
import json
import sys

QUARRY = "C:/Users/eman5/hermes-agent"
if QUARRY not in sys.path:
    sys.path.insert(0, QUARRY)
for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass

SRC = QUARRY + "/hermes_cli/plugins_cadence.py"
NAME = "plugins_cadence_live_w13_pluginscad"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    if func == "install_fingerprint":
        return live._install_fingerprint(r.get("record"))
    if func == "check_interval_hours":
        if r.get("raises"):
            def cfg(s, k):
                raise RuntimeError("boom")
        else:
            def cfg(s, k):
                return r.get("value")
        return live.check_interval_hours(config_get=cfg)
    if func == "auto_apply_enabled":
        return live.auto_apply_enabled(config_get=lambda s, k: r.get("value"))
    if func == "cached_git_updates":
        rows = r.get("rows", [])
        class FakePath:
            def read_text(self, encoding=None):
                return json.dumps(rows)
        live._results_path = lambda: FakePath()
        return live.cached_git_updates(r.get("install_metadata", {}))
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
print("ORACLE w13_pluginscad_fd67f1: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
