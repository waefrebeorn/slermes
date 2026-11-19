"""twin for tools/clarify_gateway.py:get_pending_for_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_gateway.py"
    PY_FN = "get_pending_for_session"
    C_FN = "cli_tools_clarify_gateway_get_pending_for_session"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
