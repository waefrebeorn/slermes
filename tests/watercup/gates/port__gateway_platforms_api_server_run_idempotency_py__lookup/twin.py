"""twin for gateway/platforms/api_server_run_idempotency.py:lookup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_run_idempotency.py"
    PY_FN = "lookup"
    C_FN = "lookup"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
