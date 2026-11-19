"""twin for hermes_cli/web_routers/tools.py:get_terminal_backends (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_routers/tools.py"
    PY_FN = "get_terminal_backends"
    C_FN = "get_terminal_backends"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
