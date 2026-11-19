"""twin for gateway/hooks.py:_register_builtin_hooks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hooks.py"
    PY_FN = "_register_builtin_hooks"
    C_FN = "hooks"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
