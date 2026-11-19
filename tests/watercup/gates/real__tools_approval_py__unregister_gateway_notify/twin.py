"""twin for tools/approval.py:unregister_gateway_notify (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/approval.py"
    PY_FN = "unregister_gateway_notify"
    C_FN = "approval_unregister_gateway_notify"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
