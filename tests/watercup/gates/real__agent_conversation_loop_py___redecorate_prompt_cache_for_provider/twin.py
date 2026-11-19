"""twin for agent/conversation_loop.py:_redecorate_prompt_cache_for_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_loop.py"
    PY_FN = "_redecorate_prompt_cache_for_provider"
    C_FN = "_redecorate_prompt_cache_for_provider"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
