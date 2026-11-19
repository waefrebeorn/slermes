"""twin for gateway/status.py:_read_gateway_lock_record (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "_read_gateway_lock_record"
    C_FN = "_read_gateway_lock_record"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
