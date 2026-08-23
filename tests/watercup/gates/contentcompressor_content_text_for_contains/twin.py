"""twin for contentcompressor_content_text_for_contains.

Imports LIVE agent/context_compressor.py::_content_text_for_contains and traces
the same branch ladder as the fixed C port. The V event is LIVE's real output.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_cc", os.path.join(DEV, "agent", "context_compressor.py"))
mod = ilu.module_from_spec(spec); sys.modules["wc_cc"] = mod
spec.loader.exec_module(mod)
fn = mod._content_text_for_contains

from watercup_py import Cup

def build(tokens):
    kind = tokens[0]
    if kind == "N": return None
    if kind.startswith("S:"): return kind[2:]
    items = []
    for b in tokens[1:]:
        if b.startswith("S:"): items.append(b[2:])
        elif b.startswith("TT:"): items.append({"type": "text", "text": b[3:] if len(b) > 2 else ""})
        elif b == "TN": items.append({"type": "text", "text": ""})
        elif b.startswith("TD:"): items.append({"text": b[3:] if len(b) > 2 else ""})
        elif b.startswith("TI:"): items.append({"type": "image_url", "text": b[3:]})
        elif b == "E": items.append({})
        elif b == "X": items.append(7)
    return items

def run(inputs):
    cups = []
    for raw in inputs:
        tokens = raw.split("\x1f")
        val = build(tokens)
        res = fn(val)
        c = Cup("content_text_for_contains")
        kind = tokens[0]
        if not kind:
            c.branch(True)
        else:
            c.branch(False)
            if kind == "N":
                c.branch(True)
            else:
                c.branch(False)
                if kind.startswith("S:"):
                    c.branch(True)
                else:
                    c.branch(False)
                    for b in tokens[1:]:
                        c.loop()
                        if not b:
                            c.branch(True); continue
                        c.branch(False)
                        if b.startswith("S:"):
                            c.branch(True)
                        else:
                            c.branch(False)
                            c.branch(bool(b[0] in ("T", "D")))
                        text = None
                        if b.startswith(("S:", "TT:", "TD:", "TI:")):
                            text = b[2:] if b.startswith("S:") else (b[3:] if len(b) > 2 else "")
                        keep = bool(text)
                        c.branch(keep)
        c.valstr(res or "")
        cups.append(c.finalize())
    return cups
