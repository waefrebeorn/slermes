"""twin: deterministic_call_id."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/message_sanitization.py"; PY_FN = "deterministic_call_id"
    C_FN = "agent_message_sanitization_u_deterministic_call_id"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp)
        return self._fn(d.get("fn_name",""), d.get("arguments",""), d.get("index",0))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
