#!/usr/bin/env python3
"""Differential oracle for gate w13_mainagent_07d4b1 (hermes_cli/main_agent_cmds.py)."""
import contextlib
import importlib.util
import io
import json
import os
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_mainagent_07d4b1"
sys.path.insert(0, QUARRY)


def _seed(name, **attrs):
    mod = types.ModuleType(name)
    mod.__path__ = []
    for k, v in attrs.items():
        setattr(mod, k, v)
    sys.modules[name] = mod
    parts = name.split(".")
    for i in range(1, len(parts)):
        parent = sys.modules.get(".".join(parts[:i]))
        if parent is not None and not hasattr(parent, parts[i]):
            setattr(parent, parts[i], mod)
    return mod


_seed("pm", install_hint=lambda name: "pip install hermes-agent")
_seed("hermes_cli")
_seed("agent")
MON = {}
AVAIL = False
_seed("hermes_cli.config", load_config=lambda: {"monitoring": MON})
_seed("agent.monitoring", otlp_exporter=types.SimpleNamespace(is_available=lambda: AVAIL))
CAPTURED = []
_seed("acp_adapter")
_seed("acp_adapter.entry", main=lambda flags: CAPTURED.append(list(flags)))

SRC = QUARRY + "/hermes_cli/main_agent_cmds.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)

ENV_KEYS = ("HERMES_ACCEPT_HOOKS", "HERMES_IGNORE_RULES",
            "HERMES_MODEL", "HERMES_INFERENCE_PROVIDER")


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    global MON, AVAIL
    func = r.get("func")
    if func == "dict_or_empty":
        return live._dict_or_empty(r.get("value"))
    args = types.SimpleNamespace(**(r.get("args") or {}))
    if func in ("acp_flags", "acp_env"):
        for k in ENV_KEYS:
            os.environ.pop(k, None)
        before = dict(os.environ)
        CAPTURED.clear()
        live.cmd_acp(args)
        if func == "acp_flags":
            return CAPTURED[0] if CAPTURED else []
        after = dict(os.environ)
        return {k: v for k, v in after.items() if before.get(k) != v}
    if func == "monitoring_lines":
        MON = r.get("mon") or {}
        AVAIL = bool(r.get("otlp_available"))
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            live.cmd_monitoring(types.SimpleNamespace(monitoring_action="status"))
        return buf.getvalue().splitlines()
    return None


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line.startswith("{"):
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    cases += 1
    try:
        py = compute(r)
    except Exception as exc:
        py = {"__raise__": type(exc).__name__, "msg": str(exc)}
    got = r.get("out")
    if canon(py) != canon(got):
        mism += 1
        print(f"MISMATCH {G} case={r.get('case')} func={r.get('func')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
