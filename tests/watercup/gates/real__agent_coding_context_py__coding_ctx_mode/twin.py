"""twin: agent/coding_context.py:_coding_mode, LIVE with the fixture as the config dict."""
import json as _j
import agent.coding_context as _cc
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/coding_context.py"
    PY_FN = "_coding_mode"
    C_FN = "coding_ctx_mode"
    def call(self, inp, inputs, idx=0):
        return _cc._coding_mode(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(result)
_t=_Twin(); trace=_t.trace; run=_t.run
