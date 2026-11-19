"""twin for agent/vertex_adapter.py:_refresh_credentials (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/vertex_adapter.py"
    PY_FN = "_refresh_credentials"
    C_FN = "hermes_agent_vertex_adapter_refresh_credentials"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
