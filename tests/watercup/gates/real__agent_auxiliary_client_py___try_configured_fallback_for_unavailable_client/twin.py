"""twin for agent/auxiliary_client.py:_try_configured_fallback_for_unavailable_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_try_configured_fallback_for_unavailable_client"
    C_FN = "_try_configured_fallback_for_unavailable_client"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
