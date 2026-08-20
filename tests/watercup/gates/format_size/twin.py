"""twin for format_size.

Imports the LIVE Python hermes_cli/sizefmt.py::format_bytes and traces the SAME
branch ladder as the C harness, so the cup matches iff the C port executes the
identical control structure as LIVE Python.

LIVE format_bytes(n):
    if size < 1024: return f"{int(size)} B"
    for unit in ("KB","MB","GB"): size/=1024; if size<1024: return f"{size:.1f} {unit}"
    return f"{size/1024:.1f} TB"
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))  # .../slermes
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, os.path.join(DEV, "hermes_cli"))
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_sizefmt", os.path.join(DEV, "hermes_cli", "sizefmt.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_sizefmt"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        n = float(raw)
        c = Cup("format_size")
        if n < 1024.0:
            c.branch(True)
            out = f"{int(n)} B"
        else:
            c.branch(False)
            units = ["B", "KB", "MB", "GB", "TB", None]
            size = n
            u = 0
            while units[u + 1] is not None and size >= 1024.0:
                size /= 1024.0
                c.branch(True)
                u += 1
            out = f"{size:.1f} {units[u]}"
        c.valstr(out)
        cups.append(c.finalize())
    return cups
