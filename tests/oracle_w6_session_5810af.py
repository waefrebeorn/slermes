#!/usr/bin/env python3
"""Differential oracle for acp_adapter/session.py (gate w6_session_5810af)."""
import importlib.util
import json
import sys
import types

for _s in (sys.stdin, sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except Exception:
        pass
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)
hc = types.ModuleType("hermes_constants")
hc.get_hermes_home = lambda: Q + "/.hermes"
hc.translate_cwd_for_wsl_backend = lambda s: s
hc.windows_path_to_wsl = lambda s: None
sys.modules["hermes_constants"] = hc

SRC = Q + "/acp_adapter/session.py"
spec = importlib.util.spec_from_file_location("session_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["session_live"] = live
spec.loader.exec_module(live)


def sortpy(value):
    p = live._updated_at_sort_key(value)
    return "-inf" if p == float("-inf") else p


cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except ValueError:
        continue
    func = r.get("func")
    try:
        if func == "build_session_title":
            py = live._build_session_title(r.get("title"), r.get("preview"), r.get("cwd", ""))
        elif func == "format_updated_at":
            py = live._format_updated_at(r.get("value"))
        elif func == "updated_at_sort_key":
            py = sortpy(r.get("value"))
        elif func == "parse_model_config":
            py = live._parse_model_config(r.get("mc"))
        elif func == "expand_toolsets":
            py = live._expand_acp_enabled_toolsets(r.get("toolsets"), r.get("mcp_names"))
        elif func == "session_info":
            py = live._session_info(r.get("sid", ""), r.get("cwd", ""), r.get("model"),
                                    int(r.get("history_len", 0)), r.get("title"), r.get("preview"),
                                    r.get("updated_at"))
        elif func == "first_user_preview":
            py = live._first_user_preview(r.get("history", []), r.get("default_val", ""))
        else:
            continue
    except Exception as exc:
        print("LIVE ERROR %s %r: %r" % (func, r, exc), file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print("MISMATCH %s case=%s py=%r c=%r" % (func, r.get("case"), py, got), file=sys.stderr)

print("ORACLE w6_session_5810af: %d cases, %d mismatches" % (cases, mism))
sys.exit(1 if mism or not cases else 0)
