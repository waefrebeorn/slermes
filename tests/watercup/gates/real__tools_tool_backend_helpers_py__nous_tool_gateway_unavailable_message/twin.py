"""twin for tools/tool_backend_helpers.py:nous_tool_gateway_unavailable_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_backend_helpers.py"
    PY_FN = "nous_tool_gateway_unavailable_message"
    C_FN = "nous_tool_gateway_unavailable_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
