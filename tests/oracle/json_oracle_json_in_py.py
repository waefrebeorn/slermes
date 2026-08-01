#!/usr/bin/env python3
"""Python mirror for JSON-in port functions (C takes JSON string, returns scalar)."""
import sys, json, importlib.util
sys.path.insert(0, "/home/wubu/hermes-agent-dev")

func = sys.argv[1]
raw = sys.argv[2]

def load(modpath, modname):
    spec = importlib.util.spec_from_file_location(modname, modpath)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

MAP = {
    "is_command_provider_config": ("/home/wubu/hermes-agent-dev/tools/tts_tool.py", "_is_command_provider_config"),
    "get_command_tts_timeout":     ("/home/wubu/hermes-agent-dev/tools/tts_tool.py", "_get_command_tts_timeout"),
    "openrouter_model_is_free":    ("/home/wubu/hermes-agent-dev/hermes_cli/models.py", "_openrouter_model_is_free"),
    "is_signal_rate_limit_error":  ("/home/wubu/hermes-agent-dev/gateway/platforms/signal_rate_limit.py", "_is_signal_rate_limit_error"),
    "scale_to_zero_enabled":       ("/home/wubu/hermes-agent-dev/gateway/scale_to_zero.py", "scale_to_zero_enabled"),
    "messaging_is_relay_only_or_absent": ("/home/wubu/hermes-agent-dev/gateway/scale_to_zero.py", "messaging_is_relay_only_or_absent"),
    "redact_config_value": ("/home/wubu/hermes-agent-dev/hermes_cli/config.py", "redact_config_value"),
}

mp, pyf = MAP[func]
mod = load(mp, "m_" + func)
# Decide python arg: JSON string inputs -> json.loads. For signal_rate_limit,
# Python accepts str or dict; if raw is valid JSON feed the parsed value, else str.
if func == "is_signal_rate_limit_error":
    try:
        arg = json.loads(raw)
    except Exception:
        arg = raw
else:
    arg = json.loads(raw)
r = getattr(mod, pyf)(arg)
if isinstance(r, bool):
    print("1" if r else "0")
elif isinstance(r, float):
    print("%.10g" % r)
elif isinstance(r, (dict, list)):
    print(json.dumps(r, sort_keys=True))
else:
    print(r)
