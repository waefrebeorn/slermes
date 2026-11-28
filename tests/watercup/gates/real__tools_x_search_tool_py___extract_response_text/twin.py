"""twin: _extract_response_text(payload) -> text."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/x_search_tool.py"; PY_FN = "_extract_response_text"
    C_FN = "agent_x_search_tool_u_extract_response_text"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
