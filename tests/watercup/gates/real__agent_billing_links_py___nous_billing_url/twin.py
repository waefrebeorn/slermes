"""twin for agent/billing_links.py:_nous_billing_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_links.py"
    PY_FN = "_nous_billing_url"
    C_FN = "_nous_billing_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
