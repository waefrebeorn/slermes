"""twin for gateway/status.py:looks_like_gateway_command_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/status.py"
    PY_FN = "looks_like_gateway_command_line"
    C_FN = "gwstatus_looks_like_gateway_command_line"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
