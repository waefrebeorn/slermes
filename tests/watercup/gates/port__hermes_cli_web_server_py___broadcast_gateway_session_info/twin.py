"""twin for hermes_cli/web_server.py:_broadcast_gateway_session_info (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/web_server.py"
    PY_FN = "_broadcast_gateway_session_info"
    C_FN = "_broadcast_gateway_session_info"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
