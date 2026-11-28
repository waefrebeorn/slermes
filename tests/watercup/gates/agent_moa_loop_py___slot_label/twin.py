"""twin for agent/moa_loop.py:_slot_label (JSON slot)."""
import json as _j
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_slot_label"
    C_FN = "moa_u_slot_label"
    def call(self, inp, inputs, idx=0):
        return self._fn(_j.loads(inp))
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
