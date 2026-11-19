"""twin for hermes_state.py:_reclaim_idle_read_conn_anywhere (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_reclaim_idle_read_conn_anywhere"
    C_FN = "_reclaim_idle_read_conn_anywhere"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
