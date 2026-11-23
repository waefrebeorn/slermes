#!/usr/bin/env python3
"""Differential oracle for w12_cliinfomi_ea6329 (hermes_cli/cli_info_mixin.py).

t() is the identity key and _Colors uses deterministic sentinels, so the
helpers' label/line construction is compared against the live Python code.
"""
import contextlib
import importlib.util
import io
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


# Parent-package bootstrap before executing the live module by file path.
agent = _mod("agent")
agent.__path__ = []
_mod("agent.i18n", t=lambda key, **kw: key)
_mod("hermes_constants", is_termux=lambda: False)
rich = _mod("rich")
rich.__path__ = []
_mod("rich.markup", escape=lambda s: s)
_mod("utils", base_url_hostname=lambda s: "")
hermes_cli = _mod("hermes_cli")
hermes_cli.__path__ = []
_mod("hermes_cli.cli_modal_mixin", _gated_confirm=lambda *a, **k: None)
Colors = type("Colors", (), {
    "DIM": "<DIM>", "YELLOW": "<YELLOW>", "GREEN": "<GREEN>",
    "BOLD": "<BOLD>", "RESET": "<RESET>"})
_mod("hermes_cli.colors", Colors=Colors)

SRC = Q + "/hermes_cli/cli_info_mixin.py"
spec = importlib.util.spec_from_file_location("w12_cliinfomi_live", SRC)
live = importlib.util.module_from_spec(spec)
sys.modules["w12_cliinfomi_live"] = live
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
        if func == "tool_progress_label":
            py = live._tool_progress_label(r.get("mode", ""))
        elif func == "reload_mcp_choices":
            py = [list(row) for row in live._reload_mcp_choices()]
        elif func == "help_section_title":
            py = live._help_section_title(r.get("category", ""))
        elif func == "skill_line":
            py = live._skill_line(r.get("item", {}))
        elif func == "toolset_map":
            of = r.get("toolset_of", {})
            py = live._toolset_map(r.get("tools", []), r.get("availability", {}),
                                   lambda name: of.get(name))
        elif func == "ascii_box":
            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                live._ascii_box(r.get("title", ""), int(r.get("width", 0)))
            py = buf.getvalue().splitlines()
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
print(f"ORACLE w12_cliinfomi_ea6329: {cases} cases, {mism} mismatches")
sys.exit(1 if mism else 0)
