"""twin for retry_utils_ladder.

Traces LIVE agent/retry_utils.py fns with the SAME rung order as the C
harness and asserts each mirrored outcome against the LIVE result.
"""
import os, sys, importlib.util as ilu
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
SLERMES = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE))))
DEV = os.path.dirname(SLERMES)

from watercup_py import Cup

_spec = ilu.spec_from_file_location("wc_live_retry_utils",
                                    os.path.join(DEV, "agent", "retry_utils.py"))
_live = ilu.module_from_spec(_spec)
sys.modules["wc_live_retry_utils"] = _live
_spec.loader.exec_module(_live)


class _Err:
    def __init__(self, status_code, text):
        self.status_code = status_code
        self.message = text


def _trace_ra(value):
    c = Cup("retry_parse_retry_after")
    if not value:
        c.branch(True)
        label = "none"
    else:
        c.branch(False)
        stripped = value.strip()
        if not stripped:
            c.branch(True)
            label = "none"
        else:
            c.branch(False)
            try:
                secs = float(stripped)
            except (TypeError, ValueError):
                secs = None
            if secs is not None:
                c.branch(True)
                label = "secs"
            else:
                c.branch(False)
                try:
                    when = _live.parsedate_to_datetime(stripped)
                    ok = when is not None
                except (TypeError, ValueError):
                    ok = False
                c.branch(ok)
                label = "date" if ok else "invalid"

    live = _live.parse_retry_after_seconds(value)
    if label in ("secs", "date"):
        assert isinstance(live, float) and live >= 0.0, (label, live)
    else:
        assert live is None, (label, live)

    c.valstr(label)
    return c.finalize()


def _trace_zai(status, base, model, text):
    c = Cup("retry_zai_overload")
    b1 = status == 429
    c.branch(b1)
    hit = False
    if b1:
        b2 = "api.z.ai/api/coding/paas/v4" in base.lower()
        c.branch(b2)
        if b2:
            b3 = "glm-5.2" in model.lower()
            c.branch(b3)
            if b3:
                low = text.lower()
                b4 = ("1305" in low) or ("temporarily overloaded" in low)
                c.branch(b4)
                hit = b4
            else:
                hit = False
        else:
            hit = False
    live = _live.is_zai_coding_overload_error(
        base_url=base, model=model,
        error=_Err(status, text))
    assert live == hit, (live, hit)
    c.valstr("1" if hit else "0")
    return c.finalize()


def _trace_ceiling(n):
    c = Cup("retry_zai_ceiling")
    c.valstr(str(_live.zai_coding_overload_retry_ceiling(n)))
    return c.finalize()


def run(inputs):
    out = []
    for inp in inputs:
        if inp.startswith("ra:"):
            out.append(_trace_ra(inp[3:]))
        elif inp.startswith("zai:"):
            status, base, model, text = inp[4:].split("|", 3)
            out.append(_trace_zai(int(status), base, model, text))
        elif inp.startswith("ceil:"):
            out.append(_trace_ceiling(int(inp[5:])))
        else:
            raise ValueError("bad input %r" % inp)
    return out
