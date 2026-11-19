"""twin for gateway/run.py:_start_hosted_room_worker_sync (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_start_hosted_room_worker_sync"
    C_FN = "_start_hosted_room_worker_sync"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
