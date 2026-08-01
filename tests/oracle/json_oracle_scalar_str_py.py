#!/usr/bin/env python3
"""Python mirror for scalar-string port functions (single str in -> scalar out)."""
import sys, importlib.util
sys.path.insert(0, "/home/wubu/hermes-agent-dev")

func = sys.argv[1]
s = sys.argv[2]

def load(modpath, modname):
    spec = importlib.util.spec_from_file_location(modname, modpath)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

MAP = {
    "is_openai_fast_model": ("/home/wubu/hermes-agent-dev/hermes_cli/models.py", "_is_openai_fast_model"),
    "is_anthropic_fast_model": ("/home/wubu/hermes-agent-dev/hermes_cli/models.py", "_is_anthropic_fast_model"),
    "model_supports_fast_mode": ("/home/wubu/hermes-agent-dev/hermes_cli/models.py", "model_supports_fast_mode"),
    "is_github_models_base_url": ("/home/wubu/hermes-agent-dev/hermes_cli/models.py", "_is_github_models_base_url"),
    "is_loopback_hostname": ("/home/wubu/hermes-agent-dev/tools/browser_camofox.py", "_is_loopback_hostname"),
    "is_control_interrupt_message": ("/home/wubu/hermes-agent-dev/gateway/run.py", "_is_control_interrupt_message"),
    "is_auto_continue_noise": ("/home/wubu/hermes-agent-dev/gateway/run.py", "_is_auto_continue_noise"),
    "is_intentional_silence_response": ("/home/wubu/hermes-agent-dev/gateway/response_filters.py", "is_intentional_silence_response"),
    "is_partial_silence_marker": ("/home/wubu/hermes-agent-dev/gateway/response_filters.py", "is_partial_silence_marker"),
    "coerce_ssl_verify": ("/home/wubu/hermes-agent-dev/hermes_cli/config.py", "_coerce_ssl_verify"),
    "coerce_config_version": ("/home/wubu/hermes-agent-dev/hermes_cli/config.py", "_coerce_config_version"),
}
mp, pyf = MAP[func]
mod = load(mp, "m_" + func)
r = getattr(mod, pyf)(s)
if isinstance(r, bool):
    print("1" if r else "0")
else:
    print(r)
