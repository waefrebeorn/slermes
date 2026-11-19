"""twin for gateway/restart_loop_guard.py:_chain_ending_at (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/restart_loop_guard.py"
    PY_FN = "_chain_ending_at"
    C_FN = "_chain_ending_at"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
