"""twin for agent/conversation_compression.py:_cached_prompt_reflects_builtin_memory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_cached_prompt_reflects_builtin_memory"
    C_FN = "cc_cached_prompt_reflects_builtin_memory"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
