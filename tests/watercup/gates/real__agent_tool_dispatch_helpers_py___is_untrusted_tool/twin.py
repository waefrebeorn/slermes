"""twin: _is_untrusted_tool."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/tool_dispatch_helpers.py"; PY_FN = "_is_untrusted_tool"
    C_FN = "agent_tool_dispatch_helpers_u_is_untrusted_tool"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr("1" if result else "0")
_t = _Twin(); trace = _t.trace; run = _t.run
