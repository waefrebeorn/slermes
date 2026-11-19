"""twin for tools/managed_tool_gateway.py:is_managed_nous_gateway_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/managed_tool_gateway.py"
    PY_FN = "is_managed_nous_gateway_url"
    C_FN = "cli_tools_managed_tool_gateway_is_managed_nous_gateway_url"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
