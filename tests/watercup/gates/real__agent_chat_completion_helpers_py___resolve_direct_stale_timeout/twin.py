"""twin for agent/chat_completion_helpers.py:_resolve_direct_stale_timeout (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_resolve_direct_stale_timeout"
    C_FN = "_resolve_direct_stale_timeout"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
