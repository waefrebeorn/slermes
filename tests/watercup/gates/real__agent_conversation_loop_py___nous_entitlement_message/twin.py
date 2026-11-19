"""twin for agent/conversation_loop.py:_nous_entitlement_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_nous_entitlement_message"
    C_FN = "_nous_entitlement_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
