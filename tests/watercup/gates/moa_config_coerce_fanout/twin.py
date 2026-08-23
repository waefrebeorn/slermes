"""twin for moa_config_coerce_fanout.

Imports the LIVE Python hermes_cli/moa_config.py::_coerce_fanout and traces the
SAME branch ladder as the traced-C harness. The traced result is asserted equal
to the LIVE fn output on every input.
"""
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

import importlib.util as ilu
spec = ilu.spec_from_file_location("wc_moa_config", os.path.join(DEV, "hermes_cli", "moa_config.py"))
mod = ilu.module_from_spec(spec)
sys.modules["wc_moa_config"] = mod
spec.loader.exec_module(mod)

from watercup_py import Cup


def _traced_coerce_int(c, v, defval):
    """_coerce_int events folded into the caller's Cup c."""
    empty = (v is None or v == "")
    c.branch(empty)
    if empty:
        assert _coerce_int_live(v, defval) == defval
        return defval
    try:
        n = int(v)
        c.branch(True)
        out = n
    except (TypeError, ValueError):
        c.branch(False)
        try:
            n = int(float(v))
            c.branch(True)
            out = n
        except (TypeError, ValueError):
            c.branch(False)
            out = defval
    assert _coerce_int_live(v, defval) == out
    return out


def _coerce_int_live(v, defval):
    if v is None or v == "":
        return defval
    try:
        return int(v)
    except (TypeError, ValueError):
        try:
            return int(float(v))
        except (TypeError, ValueError):
            return defval


def traced(value):
    c = Cup("coerce_fanout")
    # string form only (dict mapping form is normalized by callers)
    c.branch(False)  # isinstance(value, dict) -> False
    mode = str(value or "").strip().lower()
    canon = mode in ("per_iteration", "user_turn")
    c.branch(canon)
    if canon:
        out = mode
    else:
        c.branch(mode.startswith("every_n"))
        if mode.startswith("every_n"):
            _, sep, rest = mode.partition(":")
            has_sep = bool(sep)
            c.branch(has_sep)
            n = _traced_coerce_int(c, rest.strip(), 0) if has_sep else 0
            c.branch(n >= 2)
            if n >= 2:
                out = f"every_n:{n}"
                c.valstr(out)
                live = mod._coerce_fanout(value)
                assert out == live, f"{out!r} != LIVE {live!r}"
                return c.finalize()
            c.branch(n == 1)
            if n == 1:
                out = "per_iteration"
            else:
                out = None
        else:
            out = None
    if out is None:
        out = "user_turn"
    c.valstr(out)
    live = mod._coerce_fanout(value)
    assert out == live, f"{out!r} != LIVE {live!r}"
    return c.finalize()


def run(inputs):
    return [traced(v) for v in inputs]
