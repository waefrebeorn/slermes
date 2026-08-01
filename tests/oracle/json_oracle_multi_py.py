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
