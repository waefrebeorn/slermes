#!/usr/bin/env python3
"""sta_oracle_plugin_prompt_sections.py — LIVE-Python oracle for
plugin_prompt_sections.c (format + restore round-trip).

Uses the LIVE hermes_cli.plugins formatters and the LIVE
agent/system_prompt.py _restore_plugin_prompt_sections.
"""
import json
import os
import sys

sys.path.insert(0, "/home/wubu/hermes-agent-dev")
sys.path.insert(0, os.path.join("/home/wubu/hermes-agent-dev", "slermes"))

from hermes_cli import plugins as PL  # noqa: E402
import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location(
    "wc_live_sp",
    "/home/wubu/hermes-agent-dev/agent/system_prompt.py")
# system_prompt.py imports heavy agent deps at module scope — extract just
# the restore function via AST exec with stubbed globals.
import ast  # noqa: E402
import re  # noqa: E402

_src = open("/home/wubu/hermes-agent-dev/agent/system_prompt.py",
            encoding="utf-8").read()
_tree = ast.parse(_src)
_pieces = []
for node in _tree.body:
    if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) \
            and node.name in ("_restore_plugin_prompt_sections",
                              "restore_plugin_prompt_sections"):
        _pieces.append(ast.get_source_segment(_src, node))
    elif isinstance(node, ast.Assign):
        t = node.targets[0]
        if isinstance(t, ast.Name) and t.id == "_PLUGIN_SECTION_FRAME_RE":
            _pieces.append(ast.get_source_segment(_src, node))

_ns = {
    "re": __import__("re"),
    "MAX_SYSTEM_PROMPT_SECTION_CHARS": PL.MAX_SYSTEM_PROMPT_SECTION_CHARS,
    "PLUGIN_SECTIONS_START": PL.PLUGIN_SECTIONS_START,
    "PLUGIN_SECTIONS_END": PL.PLUGIN_SECTIONS_END,
}
exec("from typing import Any, Dict, List, Optional, Tuple\nimport re\n"
     f"MAX_SYSTEM_PROMPT_SECTION_CHARS = {PL.MAX_SYSTEM_PROMPT_SECTION_CHARS}\n"
     f"PLUGIN_SECTIONS_START = {PL.PLUGIN_SECTIONS_START!r}\n"
     f"PLUGIN_SECTIONS_END = {PL.PLUGIN_SECTIONS_END!r}\n"
     + "\n\n".join(_pieces), _ns)

_restore = _ns["_restore_plugin_prompt_sections"]


class Section:
    def __init__(self, sid, content):
        self.id = sid
        self.content = content


def main():
    cases = json.load(open(sys.argv[1], encoding="utf-8"))
    for c in cases:
        op = c["op"]
        if op == "format":
            sections = [Section(s["id"], s["content"]) for s in c["sections"]]
            print(json.dumps(PL.format_system_prompt_sections(sections)))
        elif op == "restore":
            prompt = c["prompt"]
            restored = _restore(prompt)
            out = [{"id": s.id, "content": s.content} for s in restored]
            print(json.dumps(out))


if __name__ == "__main__":
    main()
