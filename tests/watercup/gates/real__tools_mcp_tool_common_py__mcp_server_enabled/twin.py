"""twin: tools/mcp_tool_common.py:mcp_server_enabled."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool_common.py"; PY_FN = "mcp_server_enabled"
    C_FN = "w8_mcptoolcom_f3c09b_mcp_server_enabled"
    def call(self, inp, inputs, idx=0): return self._fn(_j.loads(inp))
    def norm(self, c, result): c.valstr("True" if result else "False")
_t=_Twin(); trace=_t.trace; run=_t.run
