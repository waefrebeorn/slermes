"""twin for hermes_cli/middleware.py:_get_middleware_callbacks (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/middleware.py"
    PY_FN = "_get_middleware_callbacks"
    C_FN = "hermes_cli_middleware_u_get_middleware_callbacks"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
