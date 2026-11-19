"""twin for tools/tirith_security.py:is_platform_supported (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tirith_security.py"
    PY_FN = "is_platform_supported"
    C_FN = "is_platform_supported"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
