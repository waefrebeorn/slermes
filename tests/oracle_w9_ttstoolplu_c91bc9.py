#!/usr/bin/env python3
"""Differential oracle for tools/tts_tool_plugins.py (gate w9_ttstoolplu_c91bc9)."""
import importlib.util, json, sys, types
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

tools = _mod("tools"); tools.__path__ = [Q + "/tools"]
_mod("tools.tts_command_provider", BUILTIN_TTS_PROVIDERS=frozenset(),
     DEFAULT_COMMAND_TTS_OUTPUT_FORMAT="mp3",
     _get_named_provider_config=lambda cfg, key: ({"type": "command"} if _STATE.get("cmd") else {}),
     _is_command_provider_config=lambda c: bool(c) and c.get("type") == "command")
_mod("tools.tool_backend_helpers", NOUS_MANAGED_PROVIDER="nous")
_STATE = {}

NAME = "w9_ttstoolplu_c91bc9_live"
spec = importlib.util.spec_from_file_location(NAME, Q + "/tools/tts_tool_plugins.py")
live = importlib.util.module_from_spec(spec)
sys.modules[NAME] = live
spec.loader.exec_module(live)

def compute(r):
    f = r.get("func")
    builtins = set(r.get("builtins", []))
    nous = r.get("nous", "nous")
    if f == "route_key":
        _STATE["cmd"] = bool(r.get("has_command"))
        live.BUILTIN_TTS_PROVIDERS = builtins
        live.NOUS_MANAGED_PROVIDER = nous
        return live._plugin_route_key(r.get("provider", ""), {})
    if f == "voice_kwargs":
        return live._plugin_voice_kwargs(r.get("config"))
    if f == "voice_compatible":
        live.BUILTIN_TTS_PROVIDERS = builtins
        live._lookup_plugin_provider = lambda key, discover=False: types.SimpleNamespace(
            voice_compatible=bool(r.get("available")))
        return live._plugin_provider_is_voice_compatible(r.get("provider", ""))
    if f == "is_available":
        live._lookup_plugin_provider = lambda key: types.SimpleNamespace(
            is_available=lambda: bool(r.get("available")))
        return live._plugin_provider_is_available(r.get("provider", ""))
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
        print(f"ORACLE w9_ttstoolplu_c91bc9: live raised {exc!r} case {r.get('case')}", file=sys.stderr)
        mism += 1
        continue
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH w9_ttstoolplu_c91bc9 {r.get('func')} case={r.get('case')} py={py!r} c={got!r}", file=sys.stderr)
print(f"ORACLE w9_ttstoolplu_c91bc9: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
