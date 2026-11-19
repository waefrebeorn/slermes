"""twin for agent/browser_registry.py:list_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/browser_registry.py"
    PY_FN = "list_providers"
    C_FN = "list_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
