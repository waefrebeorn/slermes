"""twin for hermes_cli/toolset_validation.py:_platform_default_toolset (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/toolset_validation.py"
    PY_FN = "_platform_default_toolset"
    C_FN = "_platform_default_toolset"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
