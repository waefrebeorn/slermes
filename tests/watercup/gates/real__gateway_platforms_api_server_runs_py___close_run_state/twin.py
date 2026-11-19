"""twin for gateway/platforms/api_server_runs.py:_close_run_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_runs.py"
    PY_FN = "_close_run_state"
    C_FN = "_close_run_state"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
