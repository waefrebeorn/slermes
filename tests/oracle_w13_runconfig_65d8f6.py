#!/usr/bin/env python3
"""Differential oracle for gate w13_runconfig_65d8f6 (gateway/run_config_loaders.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
QUARRY = "C:/Users/eman5/hermes-agent"
G = "w13_runconfig_65d8f6"
sys.path.insert(0, QUARRY)
NS = types.SimpleNamespace


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


for _pkg in ("gateway", "hermes_cli"):
    if _pkg not in sys.modules:
        _seed(_pkg)
_noop = lambda *a, **k: None


def cfg_get(cfg, *keys, default=None):
    if not isinstance(cfg, dict):
        return default
    node = cfg
    for key in keys:
        if not isinstance(node, dict) or key not in node:
            return default
        node = node[key]
    return node


_seed("gateway.config", Platform=NS(LOCAL="local", TELEGRAM="telegram"))
_seed("gateway.restart",
      DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT=30.0,
      DEFAULT_GATEWAY_POST_INTERRUPT_GRACE_TIMEOUT=2.0,
      DEFAULT_GATEWAY_RESTART_AFTER_TURN_TIMEOUT=30.0,
      DEFAULT_GATEWAY_RESTART_DRAIN_TIMEOUT=30.0,
      DEFAULT_GATEWAY_SIGNAL_INTERRUPT_GRACE_TIMEOUT=5.0,
      parse_cron_drain_timeout=lambda x: 30.0,
      parse_restart_after_turn_timeout=lambda x: 30.0,
      parse_restart_drain_timeout=lambda x: 30.0,
      parse_signal_interrupt_grace_timeout=lambda x: 5.0,
      launchd_service_label=lambda: None,
      read_launchd_exit_timeout_s=lambda x: None,
      resolve_launchd_capped_drain=lambda a, b: a)
_seed("gateway.session", SessionSource=type("SessionSource", (), {}))
_seed("gateway.session_state", SERVICE_TIER_UNSET=object())
_seed("hermes_cli.config", cfg_get=cfg_get, resolve_ephemeral_system_prompt_from_config=_noop)
_seed("hermes_cli.fallback_config", get_fallback_chain=lambda *a, **k: None)
_seed("utils", is_truthy_value=lambda *a, **k: False)
_seed("gateway.platforms")
_seed("gateway.platforms.base", DEFAULT_BUSY_TEXT_DEBOUNCE_SECONDS=0.35,
      DEFAULT_BUSY_TEXT_HARD_CAP_SECONDS=1.0)

SRC = QUARRY + "/gateway/run_config_loaders.py"
_spec = importlib.util.spec_from_file_location(G + "_live", SRC)
live = importlib.util.module_from_spec(_spec)
sys.modules[G + "_live"] = live
_spec.loader.exec_module(live)
MB = live.GatewayConfigLoadersMixin


def canon(o):
    if type(o) is float and o.is_integer():
        return int(o)
    if isinstance(o, dict):
        return {k: canon(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [canon(v) for v in o]
    return o


def compute(r):
    func = r.get("func")
    if func == "busy_modes":
        return list(MB._busy_modes_from_config(r.get("config"),
                    fallback_input=r.get("fallback_input"), fallback_text=r.get("fallback_text")))
    if func == "busy_timing":
        return list(MB._busy_text_timing_from_config(r.get("config")))
    if func == "human_delay":
        return MB._human_delay_from_config(r.get("config"))
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
