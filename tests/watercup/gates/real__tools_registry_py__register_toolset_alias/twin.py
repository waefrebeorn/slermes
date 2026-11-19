"""twin for tools/registry.py:register_toolset_alias (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "register_toolset_alias"
    C_FN = "register_toolset_alias"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
