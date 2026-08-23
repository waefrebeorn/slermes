"""twin for clarify_tool_flatten_choice.

Imports LIVE tools/clarify_tool.py::_flatten_choice and traces the SAME
ladder as the C harness (clarify_flatten_choice):
  B1 None / B2 unparseable / B3 string / B4 object+LOOP keys (B5 hit, B6 miss)
  / B7 array LOOP elems (B8 usable) / B9 scalar number.
Inputs are JSON-encoded choice values. Restricted to the agreed subset where
the flat C scanner and LIVE recursive flattener agree: strings, objects with
non-blank canonical values, arrays of plain non-blank strings.
"""
import os, sys, json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)

from watercup_py import Cup

spec = ilu.spec_from_file_location("wc_clarify", os.path.join(DEV, "tools", "clarify_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_clarify"] = mod
spec.loader.exec_module(mod)

KEYS = ("label", "description", "text", "title")


def trace(choice_json):
    c = Cup("flatten_choice")
    if not choice_json:                                    # B1
        c.branch(True)
        result = ""
        c.valstr(result)
        return result, c.finalize()
    c.branch(False)
    try:
        value = json.loads(choice_json)
        parsed = True
    except (json.JSONDecodeError, ValueError):
        parsed = False
    s = choice_json.lstrip()
    kind = "s" if s.startswith('"') else ("o" if s.startswith("{") else
           ("a" if s.startswith("[") else "?"))
    if not parsed or kind == "?":                          # B2
        c.branch(True)
        result = ""
        c.valstr(result)
        return result, c.finalize()
    c.branch(False)
    out = None
    if kind == "s":                                        # B3
        c.branch(True)
        out = value
    else:
        c.branch(False)
        if kind == "o":                                    # B4
            c.branch(True)
            found = False
            for key in KEYS:                               # LOOP keys
                c.loop()
                v = value.get(key) if isinstance(value, dict) else None
                if isinstance(v, str) and v != "":         # B5
                    c.branch(True)
                    out = v
                    found = True
                    break
                c.branch(False)
            if not found:                                  # B6
                c.branch(True)
                out = ""
            else:
                c.branch(False)
        else:                                              # B7 array
            c.branch(True)
            parts = []
            for el in (value if isinstance(value, list) else []):  # LOOP elems
                c.loop()
                if isinstance(el, str) and el != "":       # B8
                    c.branch(True)
                    parts.append(el)
                else:
                    c.branch(False)
            out = " ".join(parts)
    result = out.strip() if isinstance(out, str) else ""
    c.valstr(result)
    return result, c.finalize()


def run(inputs):
    cups = []
    for raw in inputs:
        live = mod._flatten_choice(json.loads(raw))
        result, cup = trace(raw)
        assert result == live, f"LIVE diverge on {raw!r}: {live!r} vs {result!r}"
        cups.append(cup)
    return cups
