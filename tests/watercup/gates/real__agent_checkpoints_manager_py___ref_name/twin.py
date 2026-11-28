"""twin: _ref_name."""
import json as _j, types
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/checkpoints/manager.py"; PY_FN = "_ref_name"
    C_FN = "agent_checkpoints_manager_u_ref_name"
    def call(self, inp, inputs, idx=0):
        return self._fn(str(inp))
    def norm(self, c, result):
        c.valstr(str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
