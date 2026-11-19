"""twin for tools/mcp_oauth.py:_is_figma_remote_mcp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "_is_figma_remote_mcp"
    C_FN = "mcpo_is_figma_remote_mcp"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
