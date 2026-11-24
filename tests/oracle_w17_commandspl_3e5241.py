#!/usr/bin/env python3
"""Differential oracle for gate w17_commandspl_3e5241 (hermes_cli/commands_platforms.py)."""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
sys.stderr.reconfigure(encoding="utf-8", errors="replace")
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


agent = _pkg("agent")
_mod("agent.i18n", t=lambda key, **kw: key)
hcli = _pkg("hermes_cli")
_mod("hermes_cli.commands", COMMAND_REGISTRY=[], _is_gateway_available=lambda c, o: False,
     _iter_plugin_command_entries=lambda: [], _resolve_config_gates=lambda: {})

NAME = "w17cpl_live"
spec = importlib.util.spec_from_file_location(
    NAME, Q + "/hermes_cli/commands_platforms.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

G = "w17_commandspl_3e5241"


def compute(r):
    inp = r.get("in") or {}
    func = r.get("func")
    if func == "requires_argument":
        return bool(live._requires_argument(inp.get("args_hint", "")))
    if func == "sanitize_telegram_name":
        return live._sanitize_telegram_name(inp.get("raw", ""))
    if func == "normalize_telegram_desc":
        return live._normalize_telegram_desc(inp.get("desc", ""))
    if func == "truncate_desc":
        return live._truncate_desc(inp.get("desc", ""), int(inp.get("limit", 20)))
    if func == "clamp_command_names":
        entries = [tuple(e) for e in (inp.get("entries") or [])]
        reserved = set(inp.get("reserved") or [])
        result = live._clamp_command_names(entries, reserved)
        return [list(row) for row in result]
    if func == "sanitized_rank":
        return live._sanitized_rank(inp.get("raw_names") or [])
    if func == "sanitize_slack_name":
        return live._sanitize_slack_name(inp.get("raw", ""))
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
    try:
        py = compute(r)
    except Exception as exc:
        mism += 1
        print(f"ORACLE {G}: live raised {exc!r} case={r.get('case')}", file=sys.stderr)
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {G} {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE {G}: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
