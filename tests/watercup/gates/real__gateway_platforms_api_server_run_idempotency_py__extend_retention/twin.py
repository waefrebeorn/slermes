"""twin for gateway/platforms/api_server_run_idempotency.py:extend_retention (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_run_idempotency.py"
    PY_FN = "extend_retention"
    C_FN = "extend_retention"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
