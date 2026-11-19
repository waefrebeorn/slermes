"""twin for hermes_state.py:quarantine_cross_process_lock (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "quarantine_cross_process_lock"
    C_FN = "quarantine_cross_process_lock"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
