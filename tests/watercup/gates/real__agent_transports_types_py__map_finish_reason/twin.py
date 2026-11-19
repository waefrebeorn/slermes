"""twin for agent/transports/types.py:map_finish_reason (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/types.py"
    PY_FN = "map_finish_reason"
    C_FN = "map_finish_reason"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
