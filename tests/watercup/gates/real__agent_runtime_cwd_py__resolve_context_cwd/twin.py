"""twin for agent/runtime_cwd.py:resolve_context_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/runtime_cwd.py"
    PY_FN = "resolve_context_cwd"
    C_FN = "resolve_context_cwd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
