"""twin for agent/conversation_loop.py:_print_nous_entitlement_guidance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_print_nous_entitlement_guidance"
    C_FN = "cvl_print_nous_entitlement_guidance"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
