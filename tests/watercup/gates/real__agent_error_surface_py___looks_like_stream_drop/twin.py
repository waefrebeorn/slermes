"""twin: _looks_like_stream_drop."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/error_surface.py"; PY_FN = "_looks_like_stream_drop"
    C_FN = "agent_error_surface_u_looks_like_stream_drop"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
