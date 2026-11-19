"""twin for gateway/restart_loop_guard.py:_state_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart_loop_guard.py"
    PY_FN = "_state_path"
    C_FN = "_state_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
