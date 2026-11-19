"""twin for tools/mcp_tool.py:_prune_dead_supervised_pgids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_prune_dead_supervised_pgids"
    C_FN = "_prune_dead_supervised_pgids"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
