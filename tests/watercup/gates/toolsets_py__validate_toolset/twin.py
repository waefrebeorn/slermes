"""twin for toolsets.py:validate_toolset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "toolsets.py"
    PY_FN = "validate_toolset"
    C_FN = "toolsets_validate"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
