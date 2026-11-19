"""twin for agent/billing_view.py:_fallback_portal_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_view.py"
    PY_FN = "_fallback_portal_url"
    C_FN = "_fallback_portal_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
