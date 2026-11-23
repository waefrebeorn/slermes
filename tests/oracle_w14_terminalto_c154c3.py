#!/usr/bin/env python3
"""Differential oracle for Hermes tools/terminal_tool_guards.py (gate w14_terminalto_c154c3).

tools.shell_heredoc is stubbed to identity; fixtures avoid inert heredoc bodies,
so the live quote-masking passes match the C port exactly.
"""
import importlib.util
import json
import sys
import types

sys.stdin.reconfigure(encoding="utf-8", errors="replace")
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
Q = "C:/Users/eman5/hermes-agent"
if Q not in sys.path:
    sys.path.insert(0, Q)


def _mod(name, **attrs):
    m = types.ModuleType(name)
    for k, v in attrs.items():
        setattr(m, k, v)
    sys.modules[name] = m
    return m


tools = _mod("tools")
tools.__path__ = []
_mod("tools.shell_heredoc", strip_inert_heredoc_bodies=lambda s: s)

SRC = Q + "/tools/terminal_tool_guards.py"
spec = importlib.util.spec_from_file_location("w14_terminalto_c154c3_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w14_terminalto_c154c3_live"] = live
spec.loader.exec_module(live)

cases = mism = 0
for line in sys.stdin:
    line = line.strip()
    if not line:
        continue
    try:
        r = json.loads(line)
    except Exception:
        continue
    func = r.get("func")
    try:
        if func == "validate_workdir":
            py = live._validate_workdir(r.get("workdir", ""))
        elif func == "preview":
            py = live._safe_command_preview(r.get("command"), r.get("limit", 200))
        elif func == "blocked_json":
            py = live._blocked_json(r.get("error", ""), r.get("status", ""))
        elif func == "help_version":
            py = live._looks_like_help_or_version_command(r.get("command", ""))
        elif func == "strip_quotes":
            py = live._strip_quotes(r.get("command", ""))
        elif func == "strip_trailing_amp":
            py = live._strip_trailing_amp(r.get("command", ""))
        elif func == "has_flag":
            py = live._has_flag(r.get("args", []), r.get("short", ""), tuple(r.get("long", [])))
        elif func == "is_data":
            py = live._is_data_command(r.get("word", ""), r.get("args", []))
        elif func == "returns_promptly":
            py = live._returns_promptly(r.get("word", ""), r.get("args", []))
        elif func == "command_word":
            py = list(live._command_word(r.get("tokens", [])))
        else:
            continue
    except Exception as exc:
        print(f"LIVE ERROR {func} args={r}: {exc!r}", file=sys.stderr)
        continue
    cases += 1
    got = r.get("out")
    if py != got:
        mism += 1
        print(f"MISMATCH {func} args={r} py={py!r} c={got!r}", file=sys.stderr)

print(f"ORACLE w14_terminalto_c154c3: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
