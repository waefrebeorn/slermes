"""twin for gateway/platforms/api_server_runs.py:_run_idempotency_scope (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_runs.py"
    PY_FN = "_run_idempotency_scope"
    C_FN = "_run_idempotency_scope"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
