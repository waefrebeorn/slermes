"""twin for skill_manager_tool_validate_category.

Imports LIVE tools/skill_manager_tool.py::_validate_category and traces the
SAME rung ladder as the C harness (skill_manager_val.c::skill_val_validate_category):

    C:   !cat -> slash -> len>64 -> bad-chars -> ok
    PY:  None -> strip -> empty -> slash -> len>64 -> regex-mismatch -> ok

Value events are class codes; the message TEXTS differ between LIVE and C
(documented real gap: C omits the category name from its messages and skips
the LIVE strip/empty and first-char regex nuances). Known dropped inputs:
whitespace-padded categories (LIVE strips, C rejects) and leading '-'/'.'
names (LIVE regex rejects, C accepts).
"""
import os, sys, re

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_skill_manager_tool",
                                   os.path.join(DEV, "tools", "skill_manager_tool.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_skill_manager_tool"] = mod
try:
    spec.loader.exec_module(mod)
except Exception:
    mod = None

from watercup_py import Cup

MAX_NAME_LENGTH = 64
VALID_NAME_RE = re.compile(r"^[a-z0-9][a-z0-9._-]*$")


def classify_live(cat):
    """Mirror the LIVE ladder, returning the same class codes as C."""
    if cat is None:
        return "null"
    cat = cat.strip()
    if not cat:
        return "empty"          # LIVE-only rung (returns None); C has no strip
    if "/" in cat or "\\" in cat:
        return "slash"
    if len(cat) > MAX_NAME_LENGTH:
        return "len"
    if not VALID_NAME_RE.match(cat):
        return "chars"
    return "ok"


def run(inputs):
    cups = []
    for raw in inputs:
        c = Cup("validate_category")
        if raw == "null":
            # NULL pointer on the C side
            err = mod._validate_category(None) if mod else "sentinel"
            code = classify_live(None)
            c.branch(True)
            c.valstr(code)
        else:
            c.branch(False)
            err = mod._validate_category(raw) if mod else "sentinel"
            err = mod._validate_category(raw) if mod else "sentinel"
            code = classify_live(raw)
            if "/" in raw or "\\" in raw:
                c.branch(True)
                c.valstr("slash")
            elif len(raw) > MAX_NAME_LENGTH:
                c.branch(False); c.branch(True)
                c.valstr("len")
            elif not VALID_NAME_RE.match(raw):
                c.branch(False); c.branch(False); c.branch(True)
                c.valstr("chars")
            else:
                c.branch(False); c.branch(False); c.branch(False)
                c.valstr("ok")
        cups.append(c.finalize())
    return cups
