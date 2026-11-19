"""twin for agent/delegation_context.py:exit_non_dispatcher_owned_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/delegation_context.py"
    PY_FN = "exit_non_dispatcher_owned_context"
    C_FN = "exit_non_dispatcher_owned_context"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
