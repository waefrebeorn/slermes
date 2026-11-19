"""twin for agent/prompt_cache_boundary.py:find_stable_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_cache_boundary.py"
    PY_FN = "find_stable_prefix"
    C_FN = "find_stable_prefix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
