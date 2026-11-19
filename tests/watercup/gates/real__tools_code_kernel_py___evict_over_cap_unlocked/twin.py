"""twin for tools/code_kernel.py:_evict_over_cap_unlocked (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/code_kernel.py"
    PY_FN = "_evict_over_cap_unlocked"
    C_FN = "_evict_over_cap_unlocked"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
