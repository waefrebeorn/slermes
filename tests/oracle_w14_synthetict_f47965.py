#!/usr/bin/env python3
"""Differential oracle for w14_synthetict_f47965 (tui_gateway/synthetic_turn.py)."""
import importlib.util
import json
import sys
import types

try:
    sys.stdin.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
except Exception:
    pass

Q = "C:/Users/eman5/hermes-agent"
S = Q + "/tui_gateway/synthetic_turn.py"
if Q not in sys.path:
    sys.path.insert(0, Q)
if "tui_gateway" not in sys.modules:
    _pkg = types.ModuleType("tui_gateway")
    _pkg.__path__ = [Q + "/tui_gateway"]
    sys.modules["tui_gateway"] = _pkg
_em = types.ModuleType("tui_gateway._env")
_em.env_float = lambda k, d: d
_em.env_int = lambda k, d: d
sys.modules["tui_gateway._env"] = _em

spec = importlib.util.spec_from_file_location("tui_gateway.synthetic_turn_w14", S)
live = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line: continue
    try: r = json.loads(line)
    except Exception: continue
    func = r.get("func")
    try:
        if func == "synth_turn_armed":
            ev = r.get("env_value")
            live.os = types.SimpleNamespace(environ={"HERMES_ISO_CERTIFY_SYNTH_TURN": ev} if ev is not None else {})
            py = live.synth_turn_armed()
        elif func == "parse_spec":
            defaults = dict(r.get("defaults") or {})
            live._env_float = lambda k, d, _d=defaults: _d.get(k, d)
            live._env_int = lambda k, d, _d=defaults: _d.get(k, d)
            py = live.SyntheticHeavyAgent._parse_spec(r.get("message"))
        elif func == "maybe_build_agent":
            live.synth_turn_armed = lambda: bool(r.get("armed"))
            agent = live.maybe_build_synthetic_agent(r.get("sid", "s1"), r.get("model_override"))
            py = agent.model if agent is not None else None
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} {r.get('case')}: {exc!r}", file=sys.stderr)
        mism += 1; continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_synthetict_f47965: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
