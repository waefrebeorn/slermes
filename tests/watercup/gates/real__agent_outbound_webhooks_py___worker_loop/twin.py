"""twin for agent/outbound_webhooks.py:_worker_loop (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/outbound_webhooks.py"
    PY_FN = "_worker_loop"
    C_FN = "_worker_loop"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
