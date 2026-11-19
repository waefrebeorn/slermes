"""twin for agent/transports/codex.py:_bounded_prompt_cache_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "_bounded_prompt_cache_key"
    C_FN = "_bounded_prompt_cache_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
