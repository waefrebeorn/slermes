"""twin for agent/transports/chat_completions.py:_add_prompt_cache_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/chat_completions.py"
    PY_FN = "_add_prompt_cache_key"
    C_FN = "_add_prompt_cache_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
