"""twin for gateway/run.py:_start_loop_heartbeat_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_start_loop_heartbeat_task"
    C_FN = "grun_u_start_loop_heartbeat_task"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
