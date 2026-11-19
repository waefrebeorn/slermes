"""twin for agent/browser_provider.py:provider_name (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/browser_provider.py"
    PY_FN = "provider_name"
    C_FN = "provider_name"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
