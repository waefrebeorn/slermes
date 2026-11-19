"""twin for tools/delegate_tool.py:_capture_gateway_steer_authority (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_capture_gateway_steer_authority"
    C_FN = "dlg_capture_gateway_steer_authority"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
