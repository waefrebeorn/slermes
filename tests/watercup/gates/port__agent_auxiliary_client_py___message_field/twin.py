"""twin: _message_field(msg, name) -> dict.get / getattr(..., None)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"; PY_FN = "_message_field"
    C_FN = "acf_message_field"
    def call(self, inp, inputs, idx=0):
        d=_j.loads(inp); return self._fn(d.get("msg"), d.get("name",""))
    def norm(self, c, result):
        c.valstr("" if result is None else result)
_t=_Twin(); trace=_t.trace; run=_t.run
