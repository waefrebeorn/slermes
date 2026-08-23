"""twin for clarify_gateway_coerce_multi_select_text.

Imports LIVE tools/clarify_gateway.py::_coerce_multi_select_text and traces the
SAME tokenization/resolution ladder as the C port. Input encoding:
choicesCSV\x1freplyText. The final selection comes from LIVE itself.
"""
import os, sys
from types import SimpleNamespace
HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_clarify", os.path.join(DEV, "tools", "clarify_gateway.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_clarify"] = mod
spec.loader.exec_module(mod)

import json as _json
from watercup_py import Cup

def run(inputs):
    cups = []
    for raw in inputs:
        choices_csv, _, text = raw.partition("\x1f")
        choices = [c for c in choices_csv.split(",") if c]
        entry = SimpleNamespace(choices=choices)
        c = Cup("coerce_multi_select_text")
        out = []
        ok = True
        if not text or not choices:
            c.branch(True); ok = False
        else:
            c.branch(False)
            work = text.strip()
            if "," in work:
                c.branch(True)
                tokens = [t.strip() for t in work.split(",") if t.strip()]
            else:
                c.branch(False)
                parts = work.split()
                allnum = len(parts) > 0 and all(p.isdigit() for p in parts)
                if len(parts) > 1 and allnum:
                    c.branch(True)
                    tokens = list(parts)
                else:
                    c.branch(False)
                    tokens = parts[:1]
            for tok in tokens:
                if not ok:
                    break
                if tok.isdigit():
                    c.branch(True)
                    idx = int(tok)
                    if 1 <= idx <= len(choices):
                        c.branch(True)
                        out.append(choices[idx - 1])
                    else:
                        c.branch(False); ok = False
                else:
                    c.branch(False)
                    found = False
                    for ch in choices:
                        c.loop()
                        if len(tok) == len(ch):
                            c.branch(True)
                            if ch.casefold() == tok.casefold():
                                c.branch(True)
                                out.append(ch); found = True; break
                            else:
                                c.branch(False)
                        else:
                            c.branch(False)
                    if not found:
                        ok = False
        val = "None" if (not ok or not out) else "|".join(out)
        live = mod._coerce_multi_select_text(entry, text)
        if live is None:
            live_val = "None"
        else:
            parsed = _json.loads(live) if isinstance(live, str) else live
            live_val = "|".join(str(x) for x in parsed)
        assert val == live_val, f"LIVE/ladder diverge on {raw!r}: {live_val} vs {val}"
        c.valstr(val)
        cups.append(c.finalize())
    return cups
