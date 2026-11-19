"""twin for gateway/platforms/api_server.py:_resolve_max_concurrent_runs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server.py"
    PY_FN = "_resolve_max_concurrent_runs"
    C_FN = "aps_resolve_max_concurrent_runs"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
