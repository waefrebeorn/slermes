"""twin for agent/billing_links.py:_resolve_provider_link (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/billing_links.py"
    PY_FN = "_resolve_provider_link"
    C_FN = "out_url"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
