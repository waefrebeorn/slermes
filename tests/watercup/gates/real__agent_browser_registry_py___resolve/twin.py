"""twin for agent/browser_registry.py:_resolve (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/browser_registry.py"
    PY_FN = "_resolve"
    C_FN = "_resolve"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
