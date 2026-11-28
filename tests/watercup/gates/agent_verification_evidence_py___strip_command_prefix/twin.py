"""twin for agent/verification_evidence.py:_strip_command_prefix (JSON array)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/verification_evidence.py"
    PY_FN = "_strip_command_prefix"
    C_FN = "vev_u_strip_command_prefix"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr(_j.dumps(list(result), separators=(",", ":")))
_t = _Twin(); trace = _t.trace; run = _t.run
