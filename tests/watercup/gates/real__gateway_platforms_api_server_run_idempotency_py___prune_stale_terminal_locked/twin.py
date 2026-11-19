"""twin for gateway/platforms/api_server_run_idempotency.py:_prune_stale_terminal_locked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/api_server_run_idempotency.py"
    PY_FN = "_prune_stale_terminal_locked"
    C_FN = "_prune_stale_terminal_locked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
