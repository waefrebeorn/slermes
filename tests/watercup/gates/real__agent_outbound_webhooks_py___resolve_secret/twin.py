"""twin for agent/outbound_webhooks.py:_resolve_secret (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/outbound_webhooks.py"
    PY_FN = "_resolve_secret"
    C_FN = "_resolve_secret"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
