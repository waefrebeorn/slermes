"""twin for hermes_cli/middleware.py:_has_middleware (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/middleware.py"
    PY_FN = "_has_middleware"
    C_FN = "plug_has_middleware"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
