"""twin for prompt_caching_apply_anthropic_cache_control.

Imports LIVE agent/prompt_caching.py and traces the SAME branch ladder
rung-order as the C harness (traced copy of pca_apply_anthropic_cache_control),
using the LIVE module's own helpers.
"""
import os, sys, copy, json as _json
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from watercup_py import Cup

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_pc", os.path.join(DEV, "agent", "prompt_caching.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_pc"] = mod
spec.loader.exec_module(mod)


def marked(msg):
    if not isinstance(msg, dict):
        return False
    if "cache_control" in msg:
        return True
    content = msg.get("content")
    if isinstance(content, list):
        return any(isinstance(p, dict) and "cache_control" in p for p in content)
    return False


def run(inputs):
    cups = []
    for raw in inputs:
        native_s, prefix_s, msgs_json = raw.split("|", 2)
        native = native_s == "1"
        prefix = None if prefix_s == "-" else prefix_s
        api_messages = _json.loads(msgs_json)
        c = Cup("pca_apply_anthropic_cache_control")
        if not api_messages or not isinstance(api_messages, list):
            c.branch(True)                                   # B0
            c.valstr("")
            cups.append(c.finalize())
            continue
        c.branch(False)
        import copy as _copy
        messages = list(api_messages)
        marker = mod._build_marker("5m")

        breakpoints_used = 0
        if isinstance(messages[0], dict) and messages[0].get("role") == "system":
            c.branch(True)                                   # B1
            messages[0] = copy.deepcopy(messages[0])
            breakpoints_used = mod._apply_system_cache_markers(
                messages[0], marker, prefix,
                native_anthropic=native, mark_suffix=True, fallback_to_whole=True)
        else:
            c.branch(False)                                  # B1
        c.valstr(str(breakpoints_used))

        remaining = 4 - breakpoints_used
        non_sys = []
        for i, msg in enumerate(messages):
            c.loop()                                         # L (scan)
            if not isinstance(msg, dict):
                c.branch(False)                              # B2
                continue
            c.branch(True)                                   # B2
            role = msg.get("role")
            is_non_sys = role != "system"
            c.branch(is_non_sys)                             # B3
            if is_non_sys:
                can = mod._can_carry_marker(msg, native_anthropic=native)
                c.branch(can)                                # B4
                if can:
                    non_sys.append(i)

        start = len(non_sys) - remaining
        c.branch(start < 0)                                  # B5
        if start < 0:
            start = 0
        for idx in non_sys[start:]:
            c.loop()                                         # L (apply)
            messages[idx] = copy.deepcopy(messages[idx])
            mod._apply_cache_marker(messages[idx], marker,
                                    native_anthropic=native)

        enc = "|".join(
            "%s:%s" % (m.get("role") if isinstance(m, dict) else "?",
                       "M" if marked(m) else "-")
            for m in messages)
        c.valstr(enc)
        cups.append(c.finalize())
    return cups
