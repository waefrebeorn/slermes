#!/usr/bin/env python3
"""Python mirror for multi-arg port functions (deterministic string-math)."""
import sys, importlib.util
sys.path.insert(0, "/home/wubu/hermes-agent-dev")

func = sys.argv[1]
args = sys.argv[2:]

def load(modpath, modname):
    spec = importlib.util.spec_from_file_location(modname, modpath)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m

# map func -> (module_path, pyfunc, returns_float)
MAP = {
    "multiset_char_hit_ratio": ("/home/wubu/hermes-agent-dev/gateway/platforms/yuanbao_sticker.py", "_multiset_char_hit_ratio", True),
    "bigram_jaccard":          ("/home/wubu/hermes-agent-dev/gateway/platforms/yuanbao_sticker.py", "_bigram_jaccard", True),
    "longest_subsequence_ratio":("/home/wubu/hermes-agent-dev/gateway/platforms/yuanbao_sticker.py", "_longest_subsequence_ratio", True),
    "score_field":             ("/home/wubu/hermes-agent-dev/gateway/platforms/yuanbao_sticker.py", "_score_field", True),
    "env_line_defines_key":    ("/home/wubu/hermes-agent-dev/hermes_cli/config.py", "_env_line_defines_key", False),
    "web_windows_build_number":("/home/wubu/hermes-agent-dev/hermes_cli/web_server.py", "_windows_build_number", False),
    "allows_private_ip_resolution":("/home/wubu/hermes-agent-dev/tools/url_safety.py", "_allows_private_ip_resolution", False),
    "match_host_against_rule": ("/home/wubu/hermes-agent-dev/tools/website_policy.py", "_match_host_against_rule", False),
    "compact_text":            ("/home/wubu/hermes-agent-dev/gateway/platforms/yuanbao_sticker.py", "_compact_text", False),
    "is_codex_gpt54_or_gpt55": ("/home/wubu/hermes-agent-dev/agent/auxiliary_client.py", "_is_codex_gpt54_or_gpt55", False),
    "is_codex_spark":          ("/home/wubu/hermes-agent-dev/agent/auxiliary_client.py", "_is_codex_spark", False),
    "thinking_timeout_is_thinking_timeout":("/home/wubu/hermes-agent-dev/agent/thinking_timeout_guidance.py", "is_thinking_timeout", False),
    "toolset_allowed_for_platform":("/home/wubu/hermes-agent-dev/hermes_cli/tools_config.py", "_toolset_allowed_for_platform", False),
    "provider_supports_explicit_api_mode":("/home/wubu/hermes-agent-dev/hermes_cli/runtime_provider.py", "_provider_supports_explicit_api_mode", False),
    "query_matches":           ("/home/wubu/hermes-agent-dev/hermes_cli/curses_ui.py", "_query_matches", False),
}

mp, pyf, isf = MAP[func]
mod = load(mp, "m_" + func)
r = getattr(mod, pyf)(*args)
if isf:
    print("%.10g" % float(r))
elif isinstance(r, bool):
    print("1" if r else "0")
else:
    print(r)
