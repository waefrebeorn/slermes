"""twin for agent/prompt_caching.py:build_prompt_cache_plan (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_caching.py"
    PY_FN = "build_prompt_cache_plan"
    C_FN = "build_prompt_cache_plan"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
