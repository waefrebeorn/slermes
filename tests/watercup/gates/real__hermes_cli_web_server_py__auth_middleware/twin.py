"""twin for hermes_cli/web_server.py:auth_middleware (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "auth_middleware"
    C_FN = "auth_middleware"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
