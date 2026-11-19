"""twin for tools/mcp_tool.py:_acquire_lock_on_fh (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_tool.py"
    PY_FN = "_acquire_lock_on_fh"
    C_FN = "_acquire_lock_on_fh"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
