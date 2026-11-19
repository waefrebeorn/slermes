"""twin for agent/conversation_loop.py:_try_refresh_nous_paid_entitlement_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_try_refresh_nous_paid_entitlement_credentials"
    C_FN = "cvl_try_refresh_nous_paid_entitlement_credentials"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
