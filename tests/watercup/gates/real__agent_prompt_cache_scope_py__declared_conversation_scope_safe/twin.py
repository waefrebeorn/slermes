"""twin for agent/prompt_cache_scope.py:declared_conversation_scope_safe (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_cache_scope.py"
    PY_FN = "declared_conversation_scope_safe"
    C_FN = "declared_conversation_scope_safe"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
