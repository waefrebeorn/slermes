"""twin for tools/mcp_oauth.py:mark_cimd_rejected (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "mark_cimd_rejected"
    C_FN = "mcpo_mark_cimd_rejected"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
