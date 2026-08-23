"""twin for fuzzy_match_reindent_replacement.

Imports LIVE tools/fuzzy_match.py and traces the SAME branch ladder rung-order
as the C harness (fuzzy_match_reindent_replacement), using the module's own
helpers (_first_meaningful_line, _leading_whitespace) so both sides run the
real logic.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_fuzzy_match", os.path.join(DEV, "tools", "fuzzy_match.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_fuzzy_match"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def run(inputs):
    cups = []
    # each input is one case: region\x1f old\x1f new
    for raw in inputs:
        parts = raw.split("\x1f")
        while len(parts) < 3:
            parts.append("")
        file_region, old_string, new_string = parts[0], parts[1], parts[2]
        c = Cup("fuzzy_match_reindent_replacement")
        if not new_string:
            c.branch(True)                       # B0
            result = new_string
            c.valstr(result)
            cups.append(c.finalize())
            continue
        c.branch(False)
        old_first = mod._first_meaningful_line(old_string)
        file_first = mod._first_meaningful_line(file_region)
        c.valstr(old_first if old_first is not None else "<none>")
        if old_first is None or file_first is None:
            c.branch(True)                       # B1
            result = new_string
            c.valstr(result)
            cups.append(c.finalize())
            continue
        c.branch(False)
        c.valstr(file_first)
        old_indent = mod._leading_whitespace(old_first)
        file_indent = mod._leading_whitespace(file_first)
        c.valstr(str(len(old_indent)))           # old_indent (count form)
        if old_indent == file_indent:
            c.branch(True)                       # B2
            result = new_string
            c.valstr(result)
            cups.append(c.finalize())
            continue
        c.branch(False)
        out_lines = []
        for line in new_string.split("\n"):
            c.loop()                             # L
            if not line.strip():
                c.branch(True)                   # B3 blank
                out_lines.append(line)
                continue
            c.branch(False)                      # B3 not-blank
            line_indent = mod._leading_whitespace(line)
            if line_indent.startswith(old_indent):
                c.branch(True)                   # B4 swap base prefix
                out_lines.append(file_indent + line[len(old_indent):])
            else:
                c.branch(False)                  # B4 anchor to file base
                out_lines.append(file_indent + line.lstrip(" \t"))
        result = "\n".join(out_lines)
        c.valstr(result)
        cups.append(c.finalize())
    return cups
