"""twin for gateway/platforms/api_server_run_idempotency.py:acknowledge_terminal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_run_idempotency.py"
    PY_FN = "acknowledge_terminal"
    C_FN = "acknowledge_terminal"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
