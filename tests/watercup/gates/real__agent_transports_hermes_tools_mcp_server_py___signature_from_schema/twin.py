"""twin for agent/transports/hermes_tools_mcp_server.py:_signature_from_schema (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/hermes_tools_mcp_server.py"
    PY_FN = "_signature_from_schema"
    C_FN = "_signature_from_schema"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
