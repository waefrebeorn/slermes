"""twin for sessionrecap_coerce_text.

Imports LIVE hermes_cli/session_recap.py::_coerce_text and traces the same
branch ladder as the C harness over the shared \x1f-token encoding.
The final V event is the LIVE function's real output.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_session_recap", os.path.join(DEV, "hermes_cli", "session_recap.py"))
mod = ilu.module_from_spec(spec); sys.modules["wc_session_recap"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup
SEP = "\x1f"

def build(tokens):
    kind = tokens[0]
    if kind == "N": return None
    if kind.startswith("S:"): return kind[2:]
    blocks = []
    for b in tokens[1:]:
        if b.startswith("S:"): blocks.append(b[2:])
        elif b == "TN": blocks.append({"text": ""})
        elif b == "E": blocks.append({})
        elif b.startswith("T:"): blocks.append({"text": b[2:]})
    return blocks

def run(inputs):
    cups = []
    for raw in inputs:
        tokens = raw.split(SEP)
        val = build(tokens)
        c = Cup("coerce_text")
        kind = tokens[0]
        if not kind:
            c.branch(True); res = ""
        else:
            c.branch(False)
            if kind == "N":
                c.branch(True); res = mod._coerce_text(val)
            else:
                c.branch(False)
                if kind.startswith("S:") and not kind.startswith("S:" + SEP):
                    c.branch(True); res = mod._coerce_text(val)
                else:
                    c.branch(False)
                    for b in tokens[1:]:
                        c.loop()
                        if not b: c.branch(True); continue
                        c.branch(False)
                        if b.startswith("S:"):
                            c.branch(True)
                        else:
                            c.branch(False)
                            if b[0] == "T": c.branch(True)
                            else: c.branch(False)
                        is_s_block = b.startswith("S:")
                        text = b[2:] if b.startswith(("S:", "T:")) else ("" if b == "TN" else None)
                        keep = text is not None and (is_s_block or bool(text))
                        c.branch(keep)
                    res = mod._coerce_text(val)
        c.valstr(res or "")
        cups.append(c.finalize())
    return cups
