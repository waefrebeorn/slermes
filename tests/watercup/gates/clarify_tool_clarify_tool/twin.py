"""twin for clarify_tool_clarify_tool.

Traces the LIVE tools/clarify_tool.py::clarify_tool with callback=None —
the deterministic no-UI validation path — in the SAME rung order as the C
harness around src/tools/clarify_tool.c:

  B1 questions given? / B2 normalize error / B3 batch + no callback /
  B4 empty question / B5 blank-after-strip question / B6 choices given? /
  B7 not-a-list / LOOP flatten choices (drop empties) / B8 trim to
  MAX_CHOICES / B9 any kept / B10 callback None. V = result JSON string.

All decision helpers (_normalize_questions, _flatten_choice, MAX_CHOICES,
tool_error) come from the LIVE module, so semantics are LIVE; the ladder is
the shared contract.
"""
import os, sys, json as _json, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.join(DEV, "tools"))

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_clarify_tool", os.path.join(DEV, "tools", "clarify_tool.py"))
_ct = ilu.module_from_spec(_spec)
sys.modules["wc_live_clarify_tool"] = _ct
_spec.loader.exec_module(_ct)

ERR_REQUIRED = '{"error": "Question text is required."}'
ERR_NOT_LIST = '{"error": "choices must be a list of strings."}'
ERR_NO_UI = '{"error": "Clarify tool is not available in this execution context."}'


def trace(question, choices, questions):
    c = Cup("clarify_tool")
    res = None

    # B1: batch path
    if questions is not None:
        c.branch(True)
        normalized, err = _ct._normalize_questions(questions)
        if err:                                        # B2
            c.branch(True)
            res = _ct.tool_error(err)
        else:
            c.branch(False)
            if normalized:                             # B3 (callback is None)
                c.branch(True)
                res = ERR_NO_UI
            else:
                c.branch(False)                        # fall through
    else:
        c.branch(False)

    if res is None:
        # B4 / B5: question presence
        c.branch(not question)
        if not question:
            res = ERR_REQUIRED
        else:
            q = question.strip()
            c.branch(not q)
            if not q:
                res = ERR_REQUIRED
            else:
                # B6: choices given?
                if choices is not None:
                    c.branch(True)
                    if not isinstance(choices, list):      # B7
                        c.branch(True)
                        res = ERR_NOT_LIST
                    else:
                        c.branch(False)
                        kept = []
                        for raw in choices:                # LOOP flatten
                            c.loop()
                            flat = _ct._flatten_choice(raw)
                            keep = bool(flat)
                            c.branch(keep)
                            if keep:
                                kept.append(flat)
                        if len(kept) > _ct.MAX_CHOICES:    # B8 trim
                            c.branch(True)
                            kept = kept[:_ct.MAX_CHOICES]
                        else:
                            c.branch(False)
                        has_choices = bool(kept)           # B9
                        c.branch(has_choices)
                else:
                    c.branch(False)
                if res is None:                            # B10 callback None
                    res = ERR_NO_UI
    c.valstr(res)
    return res, c.finalize()


def run(inputs):
    cups = []
    for item in inputs:
        obj = _json.loads(item)
        _, cup = trace(obj.get("question"), obj.get("choices"), obj.get("questions"))
        cups.append(cup)
    return cups
