#!/usr/bin/env python3
"""Differential oracle for hermes_cli/cli_terminal_input.py (gate w17_clitermina_e2aba1).

agent.i18n and cli-level regex/module constants are stubbed; all exercised
helpers are pure and receive env/config/text explicitly from the fixture.
"""
import importlib.util
import json
import re
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


agent = _mod("agent")
agent.__path__ = []
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("cli", _BACKSLASH_LINE_CONTINUATION_RE=re.compile(r"\\[ \t]*$"))

SRC = Q + "/hermes_cli/cli_terminal_input.py"
spec = importlib.util.spec_from_file_location("w17_clitermina_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w17_clitermina_live"] = live
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
        if func == "is_ghostty":
            py = live._is_ghostty_terminal(r.get("env", {}))
        elif func == "supports_enter":
            py = live._terminal_supports_extended_enter_keys(r.get("env", {}))
        elif func == "multiline":
            py = live._cli_multiline_shortcuts_enabled(r.get("config"))
        elif func == "backslash_check":
            py = live._is_backslash_line_continuation(r.get("text", ""))
        elif func == "backslash_apply":
            py = live._apply_backslash_line_continuation(r.get("text", ""))
        elif func == "clipboard":
            py = live._should_auto_attach_clipboard_image_on_paste(r.get("text", ""))
        elif func == "statusbar":
            py = live._status_bar_visible_from_display_config(r.get("config"))
        elif func == "split_path":
            res = live._split_path_input(r.get("raw", ""))
            py = [res[0], res[1]]
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

print(f"ORACLE w17_clitermina_e2aba1: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
