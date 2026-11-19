"""twin for hermes_cli/web_server.py:_terminate_desktop_managed_gateway (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_terminate_desktop_managed_gateway"
    C_FN = "ws_terminate_desktop_managed_gateway"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
