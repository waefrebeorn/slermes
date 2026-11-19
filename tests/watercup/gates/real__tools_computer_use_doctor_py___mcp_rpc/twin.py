"""twin for tools/computer_use/doctor.py:_mcp_rpc (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/computer_use/doctor.py"
    PY_FN = "_mcp_rpc"
    C_FN = "_mcp_rpc"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
