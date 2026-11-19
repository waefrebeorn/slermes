"""twin for agent/auxiliary_client.py:_nous_portal_account_has_fresh_paid_access (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "_nous_portal_account_has_fresh_paid_access"
    C_FN = "aux__nous_portal_account_has_fresh_paid_access"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
