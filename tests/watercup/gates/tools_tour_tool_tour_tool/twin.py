"""twin for tools_tour_tool_tour_tool.

Traces the LIVE tools/tour_tool.py::tour_tool with the same rung ladder as
the C harness (which wraps the REAL src/tools/tour_tool.c port behind a
stub preview bridge). The LIVE result is classified canonically so C's
compact JSON serialization and Python's spaced dumps hash identically.
"""
import os
import sys
import json
import importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)
sys.path.insert(0, DEV)
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))  # tests/watercup

from watercup_py import Cup

_spec = ilu.spec_from_file_location(
    "wc_live_tour_tool", os.path.join(DEV, "tools", "tour_tool.py"))
_tt = ilu.module_from_spec(_spec)
sys.modules["wc_live_tour_tool"] = _tt
try:
    _spec.loader.exec_module(_tt)
    LIVE_TOUR = _tt.tour_tool
except Exception:  # pragma: no cover
    LIVE_TOUR = None


def parse(spec):
    f = (spec.split(";") + ["-"] * 8)[:8]
    action, surface, side, sel, title, text, steps_f, cbrate = f

    def opt(v):
        return None if v == "-" else v

    steps = None
    if steps_f == "@":
        steps = []
    elif steps_f != "-":
        steps = []
        for tok in steps_f.split("~"):
            if tok[0] == "t":
                steps.append("notadict")
            elif tok[1] == "1":
                steps.append({"text": "x"})
            else:
                steps.append({})
    kwargs = dict(
        action=opt(action) or "",
        surface=opt(surface),
        selector=opt(sel),
        title=opt(title),
        text=opt(text),
        side=opt(side),
        steps=steps,
        step_index=None,
    )
    canned = None
    if cbrate == "json":
        canned = '{"ok":true}'
    elif cbrate == "text":
        canned = "hello"
    if cbrate == "nocb":
        kwargs["callback"] = None
    else:
        kwargs["callback"] = (lambda *a, _raw=canned:
                              None if _raw is None else _raw)
    return kwargs


ACTIONS = ("targets", "show", "start", "next", "prev", "stop")
SURFACES = ("app", "preview")
SIDES = ("top", "right", "bottom", "left")


def classify(result):
    """Canonical outcome from a LIVE tour_tool() return string."""
    try:
        obj = json.loads(result)
    except (TypeError, ValueError):
        obj = None
    if isinstance(obj, dict):
        if set(obj) == {"error"}:
            return "err|" + obj["error"]
        if set(obj) == {"text"}:
            return "wrap"
        return "ok"
    return "wrap"


def trace(spec):
    kw = parse(spec)
    c = Cup("tour_tool")
    action = kw.get("action") or ""
    verb = str(action).strip().lower()
    where = str(kw.get("surface") or "app").strip().lower()
    side = kw.get("side")
    have_cb = kw.get("callback") is not None

    c.branch(have_cb)                                          # B1
    if not have_cb:
        c.valstr("err|tour is only available in the Hermes desktop app.")
        return "err", c.finalize()

    in_set = verb in ACTIONS
    c.branch(in_set)                                           # B2
    if not in_set:
        c.valstr("err|action must be one of: targets, show, start, next, prev, stop.")
        return "err-action", c.finalize()

    surf_ok = where in SURFACES
    c.branch(surf_ok)                                          # B3
    if not surf_ok:
        c.valstr("err|surface must be one of: app, preview.")
        return "err-surface", c.finalize()

    side_bad = side is not None and side not in SIDES
    c.branch(side_bad)                                         # B4
    if side_bad:
        c.valstr("err|side must be one of: top, right, bottom, left.")
        return "err-side", c.finalize()

    def empty(step):
        return not (step.get("selector") or step.get("title") or step.get("text"))

    show_empty = False
    if verb == "show":
        show_empty = empty({"selector": kw.get("selector"),
                            "title": kw.get("title"), "text": kw.get("text")})
    c.branch(show_empty)                                       # B5
    if show_empty:
        c.valstr("err|show needs a selector (and/or title/text for the popover).")
        return "err-show", c.finalize()

    steps = kw.get("steps")
    start_bad = False
    if verb == "start":
        start_bad = not isinstance(steps, list) or not steps
    c.branch(start_bad)                                        # B6
    if start_bad:
        c.valstr("err|start needs a non-empty steps array.")
        return "err-steps", c.finalize()

    if verb == "start":
        for i, step in enumerate(steps):                       # LOOP steps
            c.loop()
            is_obj = isinstance(step, dict)
            c.branch(not is_obj)                               # B7
            if not is_obj:
                c.valstr("err|steps[%d] must be an object." % i)
                return "err-step-obj", c.finalize()
            st_empty = empty(step)
            c.branch(st_empty)                                 # B8
            if st_empty:
                c.valstr("err|steps[%d] needs a selector and/or title/text." % i)
                return "err-step-empty", c.finalize()

    result = LIVE_TOUR(**kw)
    outcome = classify(result)

    raw_none = outcome.startswith("err|The tour request timed out")
    c.branch(raw_none)                                         # B9
    c.valstr(outcome)
    return outcome, c.finalize()


def run(inputs):
    cups = []
    for spec in inputs:
        _, cup = trace(spec)
        cups.append(cup)
    return cups
