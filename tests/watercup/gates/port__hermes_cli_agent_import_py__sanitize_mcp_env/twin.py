"""twin for hermes_cli/agent_import.py:sanitize_mcp_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_import.py"
    PY_FN = "sanitize_mcp_env"
    C_FN = "ai_sanitize_mcp_env"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
