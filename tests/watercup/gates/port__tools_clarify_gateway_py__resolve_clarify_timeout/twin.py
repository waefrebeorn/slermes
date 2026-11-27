"""twin: LIVE tools/clarify_gateway.py resolve_clarify_timeout."""
import json as _json

from watercup_py import Cup
from watercup_twin_runtime import TwinBase


class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_gateway.py"
    PY_FN = "resolve_clarify_timeout"
    C_FN = "wc_clarify_timeout"

    def trace(self, inp, inputs, idx=0):
        import tools.clarify_gateway as cg

        label = getattr(self, "_cup_label", None) or "wc_clarify_timeout"
        c = Cup(label)
        text = "" if inp is None else str(inp)
        c.branch(bool(text))                 # B1
        try:
            cfg = _json.loads(text) if text else {}
        except Exception:
            cfg = {}
        c.valstr(str(cg.resolve_clarify_timeout(cfg)))   # V1
        return c.finalize()

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
