"""twin for platforms_helpers_split_markdown_atoms.

Traces the LIVE gateway/platforms/helpers.py::split_markdown_atoms structure
with the SAME rung ladder as the C harness (which drives the REAL C port),
then takes the atom list from the LIVE function.
"""
import os
import sys
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_platforms_helpers",
    os.path.join(DEV, "gateway", "platforms", "helpers.py"))
_ph = ilu.module_from_spec(_spec)
sys.modules["wc_live_platforms_helpers"] = _ph
if _spec.loader is not None:
    _spec.loader.exec_module(_ph)


def trace(text):
    c = Cup("split_markdown_atoms")
    lines = text.split("\n")
    current = []          # shadow of current_lines for prev-table checks
    in_fence = False
    for line in lines:
        is_table = bool(line.strip()) and line.strip().startswith("|") and line.strip().endswith("|")
        prev_is_table = bool(current) and _is_table(current[-1])
        c.branch(in_fence)                                        # B1
        if in_fence:
            current.append(line)
            c.branch(line.startswith("```") and len(current) > 1)  # B2
            if line.startswith("```") and len(current) > 1:
                in_fence = False
                current = []
        elif line.startswith("```"):                              # B3
            c.branch(True)
            current = [line]
            in_fence = True
        else:
            c.branch(False)
            c.branch(is_table)                                    # B4
            if is_table:
                c.branch(bool(current) and not prev_is_table)     # B5
                if bool(current) and not prev_is_table:
                    current = []
                current.append(line)
            elif line.strip() == "":                              # B6
                c.branch(True)
                current = []
            else:
                c.branch(False)
                c.branch(bool(current) and prev_is_table)         # B7
                if bool(current) and prev_is_table:
                    current = []
                current.append(line)
    atoms = _ph.split_markdown_atoms(text)
    c.valstr("\x1f".join(a.replace("\n", "\\n") for a in atoms))
    return c.finalize()


def _is_table(line: str) -> bool:
    s = line.strip()
    return s.startswith("|") and s.endswith("|")


def run(inputs):
    return [trace(raw.replace("\\n", "\n")) for raw in inputs]
