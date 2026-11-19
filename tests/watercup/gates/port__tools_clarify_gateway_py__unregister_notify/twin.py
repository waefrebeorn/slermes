"""twin for tools/clarify_gateway.py:unregister_notify (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/clarify_gateway.py"
    PY_FN = "unregister_notify"
    C_FN = "cli_tools_clarify_gateway_unregister_notify"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
