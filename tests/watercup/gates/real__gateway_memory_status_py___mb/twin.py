"""twin: gateway/memory_status.py:_mb.

The fixture is the config token; it is decoded here so python's int-vs-float
distinction (3 vs 3.0) is exercised -- the C json node cannot represent it.
"""
import json as _j
import gateway.memory_status as _ms
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_status.py"
    PY_FN = "_mb"
    C_FN = "gw_memstat_mb_text"
    NORM_NONE = True   # the port returns the literal "None", not ""
    def call(self, inp, inputs, idx=0):
        return _ms._mb(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("None" if result is None else str(result))
_t=_Twin(); trace=_t.trace; run=_t.run
