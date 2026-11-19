"""twin for gateway/hosted_rooms.py:remote_run_receipt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "remote_run_receipt"
    C_FN = "remote_run_receipt"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
