"""twin for agent/delegation_context.py:delegated_child_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/delegation_context.py"
    PY_FN = "delegated_child_context"
    C_FN = "delegated_child_context_exit"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
