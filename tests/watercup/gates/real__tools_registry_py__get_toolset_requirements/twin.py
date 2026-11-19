"""twin for tools/registry.py:get_toolset_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/registry.py"
    PY_FN = "get_toolset_requirements"
    C_FN = "get_toolset_requirements"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
