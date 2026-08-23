"""twin for clarify_tool_normalize_questions.

Traces the LIVE tools/clarify_tool.py::_normalize_questions with the same rung
order as the C harness (see harness.c header). Uses LIVE _flatten_choice,
MAX_QUESTIONS and MAX_CHOICES so the values come from the live module.
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
    "wc_live_clarify", os.path.join(DEV, "tools", "clarify_tool.py"))
_ct = ilu.module_from_spec(_spec)
sys.modules["wc_live_clarify"] = _ct
_spec.loader.exec_module(_ct)

import json as _json


def trace(raw):
    c = Cup("normalize_questions")
    try:
        questions = _json.loads(raw)
    except Exception:
        questions = None

    def err(msg):
        c.valstr(msg)

    parsed = isinstance(questions, list)
    if questions is None or not parsed:                    # B1/B2
        c.branch(True)
        err("questions must be an array of question objects.")
        return c
    c.branch(False)
    n = len(questions)
    if n == 0:                                             # B3
        c.branch(True)
        err("null")
        return c
    c.branch(False)
    if n > _ct.MAX_QUESTIONS:                              # B4
        c.branch(True)
        err("questions supports at most %d items." % _ct.MAX_QUESTIONS)
        return c
    c.branch(False)

    for index, item in enumerate(questions):
        c.loop()
        has_choices = False
        choices = None
        if isinstance(item, str):                          # B5
            c.branch(True)
            item = {"question": item}
            text = str(item.get("question") or "").strip()
        else:
            c.branch(False)
            if not isinstance(item, dict):                 # B6
                c.branch(True)
                err('{"error": "questions[%d] must be an object with a \'question\'.",}'.replace(",}", "}") % index)
                return c
            c.branch(False)
            text = str(item.get("question") or "").strip()
        if not text:                                       # B7
            c.branch(True)
            err('{"error": "questions[%d].question must be non-empty text."}' % index)
            return c
        c.branch(False)

        raw_choices = item.get("choices")
        if raw_choices is not None:                        # B8
            c.branch(True)
            if not isinstance(raw_choices, list):          # B9
                c.branch(True)
                err('{"error": "questions[%d].choices must be a list."}' % index)
                return c
            c.branch(False)
            choices = [s for s in (_ct._flatten_choice(cv) for cv in raw_choices) if s]
            for _cv in raw_choices:                        # per-choice LOOP + skip-empty
                c.loop()
                flat = _ct._flatten_choice(_cv)
                c.branch(not flat)
            if len(choices) > _ct.MAX_CHOICES:
                choices = choices[:_ct.MAX_CHOICES]
            if not choices:                                # B10
                c.branch(True)
                choices = None
            else:
                c.branch(False)
                has_choices = True
        else:
            c.branch(False)
            c.branch(False)                                # B9 not taken
            c.branch(False)                                # B10 not taken

        model_id = str(item.get("id") or "").strip() or None
        c.branch(model_id is not None)                     # B11
        ms = bool(item.get("multi_select")) and has_choices
        c.valstr("q%d|%s|%s|%d|%s" % (
            index, model_id or "", text,
            len(choices) if choices else 0, "True" if ms else "False"))
    return c


def run(inputs):
    return [trace(raw).finalize() for raw in inputs]
