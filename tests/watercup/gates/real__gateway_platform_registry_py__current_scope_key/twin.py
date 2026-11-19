"""twin for gateway/platform_registry.py:current_scope_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platform_registry.py"
    PY_FN = "current_scope_key"
    C_FN = "current_scope_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
