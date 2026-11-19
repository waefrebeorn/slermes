"""twin for agent/portal_tags.py:reset_conversation_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/portal_tags.py"
    PY_FN = "reset_conversation_context"
    C_FN = "hermes_reset_conversation_context"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
