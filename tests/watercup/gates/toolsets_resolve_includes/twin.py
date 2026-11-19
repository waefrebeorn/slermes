"""twin for toolsets.py:resolve_toolset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "toolsets.py"
    PY_FN = "resolve_toolset"
    C_FN = "resolve_toolset"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
