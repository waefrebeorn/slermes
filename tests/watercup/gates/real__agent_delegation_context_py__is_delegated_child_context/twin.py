"""twin for agent/delegation_context.py:is_delegated_child_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/delegation_context.py"
    PY_FN = "is_delegated_child_context"
    C_FN = "is_delegated_child_context"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
