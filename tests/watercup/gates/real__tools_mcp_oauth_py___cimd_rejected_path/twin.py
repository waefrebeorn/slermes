"""twin for tools/mcp_oauth.py:_cimd_rejected_path (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/mcp_oauth.py"
    PY_FN = "_cimd_rejected_path"
    C_FN = "_cimd_rejected_path"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
