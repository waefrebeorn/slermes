"""twin for gateway/platforms/api_server_runs.py:_sweep_orphaned_runs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_runs.py"
    PY_FN = "_sweep_orphaned_runs"
    C_FN = "_sweep_orphaned_runs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
