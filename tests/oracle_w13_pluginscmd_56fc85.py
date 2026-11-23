#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_cmd_git.py (gate w13_pluginscmd_56fc85)."""
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


_stub("hermes_cli._subprocess_compat", noninteractive_git_env=lambda: {})
_stub("hermes_constants", get_hermes_home=lambda: None)
_stub("utils", atomic_write_text=lambda *a, **k: None)


class PluginOperationError(Exception):
    pass


_stub("hermes_cli.plugins_cmd", PluginOperationError=PluginOperationError,
      _read_install_metadata=lambda: {})

SRC = QUARRY + "/hermes_cli/plugins_cmd_git.py"
NAME = "plugins_cmd_git_live_w13_pluginscmd"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)


def compute(r):
    func = r.get("func")
    metadata = r.get("metadata")
    if func == "pinned_revision":
        return live.pinned_revision(r.get("name", ""), metadata=metadata)
    if func == "pin_annotation":
        return live._pin_annotation(r.get("name", ""), r.get("metadata", {}))
    if func == "normalize_exact_revision":
        try:
            return {"ok": True, "value": live._normalize_exact_revision(r.get("ref"))}
        except Exception as exc:
            return {"ok": False, "error": str(exc)}
    if func == "scrub_git_url":
        return live._scrub_git_url(r.get("git_url", ""))
    if func == "canonical_source":
        return live._canonical_source(r.get("git_url", ""), r.get("subdir"))
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
print("ORACLE w13_pluginscmd_56fc85: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
