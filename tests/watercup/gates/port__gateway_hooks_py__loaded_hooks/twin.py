"""twin for gateway/hooks.py:loaded_hooks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hooks.py"
    PY_FN = "loaded_hooks"
    C_FN = "loaded_hooks"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
