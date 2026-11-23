#!/usr/bin/env python3
"""Differential oracle for acp_adapter/commands.py (gate w7_commands_7ff5d7).

Loads the LIVE module with acp.schema / acp_adapter.session stubbed and replays
the command registry, the slash parse contract and the two pure text renders.
"""
import importlib.util, json, sys, types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")

Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)

acp = types.ModuleType("acp")
acp.__path__ = [Q + "/acp"]
sys.modules["acp"] = acp
schema = types.ModuleType("acp.schema")
for _n in ("AvailableCommand", "AvailableCommandsUpdate", "UnstructuredCommandInput"):
    setattr(schema, _n, type(_n, (), {}))
sys.modules["acp.schema"] = schema
acp.schema = schema

acp_adapter = types.ModuleType("acp_adapter")
acp_adapter.__path__ = [Q + "/acp_adapter"]
sys.modules["acp_adapter"] = acp_adapter
session = types.ModuleType("acp_adapter.session")
session.SessionState = type("SessionState", (), {})
session._expand_acp_enabled_toolsets = lambda x: x
sys.modules["acp_adapter.session"] = session
acp_adapter.session = session

SRC = Q + "/acp_adapter/commands.py"
spec = importlib.util.spec_from_file_location("w7_commands_7ff5d7_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w7_commands_7ff5d7_live"] = live
spec.loader.exec_module(live)


class Mixin:
    _COMMANDS = live.SlashCommandsMixin._COMMANDS

    _cmd_help = live.SlashCommandsMixin._cmd_help
    _cmd_model = live.SlashCommandsMixin._cmd_model


def parse(text):
    parts = text.split(maxsplit=1)
    cmd = parts[0].lstrip("/").lower() if parts else ""
    args = parts[1].strip() if len(parts) > 1 else ""
    return {"cmd": cmd, "args": args, "known": cmd in live.SlashCommandsMixin._COMMANDS}


def compute(r):
    func = r.get("func")
    if func == "commands_table":
        return {k: list(v) for k, v in live.SlashCommandsMixin._COMMANDS.items()}
    if func == "mid_turn_blocked":
        return sorted(live._MID_TURN_BLOCKED_COMMANDS)
    if func == "parse_slash":
        return parse(r.get("text", "") or "")
    if func == "help_text":
        return Mixin()._cmd_help("", None)
    if func == "model_status":
        agent = types.SimpleNamespace()
        if r.get("fallback"):
            agent.model = r["fallback"]
        if r.get("provider"):
            agent.provider = r["provider"]
        state = types.SimpleNamespace(model=r.get("model") or None, agent=agent)
        return Mixin()._cmd_model("", state)
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
        print(f"MISMATCH {r.get('func')} args={r} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w7_commands_7ff5d7: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
