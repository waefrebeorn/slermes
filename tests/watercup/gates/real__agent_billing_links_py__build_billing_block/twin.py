"""twin for agent/billing_links.py:build_billing_block (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_links.py"
    PY_FN = "build_billing_block"
    C_FN = "build_billing_block"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
