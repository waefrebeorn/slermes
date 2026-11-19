"""twin for agent/delegation_context.py:delegated_child_subprocess_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/delegation_context.py"
    PY_FN = "delegated_child_subprocess_env"
    C_FN = "delegated_child_subprocess_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
