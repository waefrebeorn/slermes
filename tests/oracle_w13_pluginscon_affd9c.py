#!/usr/bin/env python3
"""Differential oracle for hermes_cli/plugins_context.py (gate w13_pluginscon_affd9c)."""
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


class _MixA:
    pass


class _MixB:
    pass


_stub("hermes_cli.plugins_capability_ctx", PluginCapabilityCtxMixin=_MixA)
_stub("hermes_cli.plugins_dispatch", DEFAULT_SYSTEM_PROMPT_SECTION_MAX_CHARS=2000)
_stub("hermes_cli.plugins_idle_tasks", PluginIdleTaskCtxMixin=_MixB)
_stub("hermes_cli.plugins_ledger", PluginRegistration=dict)
_stub("hermes_cli.plugins_load_guard", _ignore_after_abandoned_load=lambda f: f,
      ignore_registrars_after_abandoned_load=lambda f: f)
_stub("hermes_cli.plugins_loader", _serialized_replacement=lambda f: f)
_stub("hermes_cli.plugins_manifest", PluginManifest=dict)
_stub("hermes_cli.plugins_registrars", register_scoped_provider=lambda *a, **k: None,
      scoped_provider_registrar=lambda *a, **k: None)
_stub("hermes_cli.plugins_state", PluginState=dict)

SRC = QUARRY + "/hermes_cli/plugins_context.py"
NAME = "plugins_context_live_w13_pluginscon"
spec = importlib.util.spec_from_file_location(NAME, SRC)
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)
# _mcp_envelope only uses the facade for json.loads.
live._facade = lambda: types.SimpleNamespace(json=json)


def compute(r):
    if r.get("func") == "mcp_envelope":
        return live.PluginContext._mcp_envelope(r.get("raw"))
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
print("ORACLE w13_pluginscon_affd9c: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
