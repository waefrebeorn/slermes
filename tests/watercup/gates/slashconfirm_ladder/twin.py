"""twin for slashconfirm_ladder.

Traces the LIVE tools/slash_confirm.py::resolve / clear_if_stale decision
ladder with the SAME rung order as the C harness, driving the module-level
_pending dict with an injected created_at so both sides see identical clock
state. LIVE results are asserted against the mirrored class.
"""
import os, sys, time, asyncio, importlib.util as ilu

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_slash_confirm",
                                    os.path.join(DEV, "tools", "slash_confirm.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_slash_confirm"] = _live
_spec.loader.exec_module(_live)


async def _handler(choice):
    return "ran:%s" % choice


def _seed(key, cid, age):
    _live._pending[key] = {
        "confirm_id": cid,
        "command": "reload-mcp",
        "handler": _handler,
        "created_at": time.time() - age,
    }


def _trace_resolve(key, cid, choice, age, timeout):
    _seed(key, cid, age)
    c = Cup("slashconfirm_resolve")
    entry = _live._pending.get(key)

    found = entry is not None
    c.branch(found)                                   # B1
    if not found:
        label = "missing"
    else:
        id_match = entry.get("confirm_id") == cid
        c.branch(id_match)                            # B2
        if not id_match:
            label = "mismatch"
        else:
            in_time = (time.time() - float(entry.get("created_at", 0))) <= timeout
            c.branch(in_time)                         # B3
            if not in_time:
                label = "stale"
            else:
                has_handler = bool(entry.get("handler"))
                c.branch(has_handler)                 # B4
                label = "ran" if has_handler else "nohandler"

    # LIVE resolve performs its own pop-then-run; don't pre-pop here.

    out = asyncio.run(_live.resolve(key, cid, choice, timeout=timeout))
    expected = {"ran": "ran:%s" % choice, "stale": None, "mismatch": None,
                "missing": None, "nohandler": None}[label]
    assert out == expected or (label == "ran" and isinstance(out, str)), \
        (label, out, expected)
    c.valstr(label)
    return c.finalize()


def _trace_stale(key, age, timeout):
    _seed(key, "id-1", age)
    c = Cup("slashconfirm_clear_if_stale")
    entry = _live._pending.get(key)
    found = entry is not None
    c.branch(found)                                   # B1
    stale = False
    if found:
        stale = (time.time() - float(entry.get("created_at", 0))) > timeout
        c.branch(stale)                               # B2
    else:
        c.branch(False)
    live = _live.clear_if_stale(key, timeout=timeout)
    assert live == stale, (live, stale)
    c.valstr("1" if stale else "0")
    return c.finalize()


def run(inputs):
    out = []
    for inp in inputs:
        if inp.startswith("res:"):
            key, cid, choice, rest = inp[4:].split("|", 3)
            if "|" in rest:
                age, timeout = rest.split("|", 1)
            else:
                age, timeout = rest, "300"
            out.append(_trace_resolve(key, cid, choice, float(age), float(timeout)))
        elif inp.startswith("stale:"):
            key, rest = inp[6:].split("|", 1)
            if "|" in rest:
                age, timeout = rest.split("|", 1)
            else:
                age, timeout = rest, "300"
            out.append(_trace_stale(key, float(age), float(timeout)))
        else:
            raise ValueError("bad input %r" % inp)
    return out
