"""twin for agent/browser_provider.py:emergency_cleanup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/browser_provider.py"
    PY_FN = "emergency_cleanup"
    C_FN = "browser_provider_emergency_cleanup"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
