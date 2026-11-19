"""twin for gateway/memory_status.py:_read_heartbeat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/memory_status.py"
    PY_FN = "_read_heartbeat"
    C_FN = "_read_heartbeat"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
