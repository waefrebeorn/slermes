"""twin for tools/lazy_deps.py:_warm_installed_bytecode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/lazy_deps.py"
    PY_FN = "_warm_installed_bytecode"
    C_FN = "_warm_installed_bytecode"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
