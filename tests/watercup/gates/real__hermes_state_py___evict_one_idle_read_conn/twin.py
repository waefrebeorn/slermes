"""twin for hermes_state.py:_evict_one_idle_read_conn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_evict_one_idle_read_conn"
    C_FN = "_evict_one_idle_read_conn"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
