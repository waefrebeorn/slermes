"""twin for agent/prompt_cache_boundary.py:clear_stable_prefixes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_cache_boundary.py"
    PY_FN = "clear_stable_prefixes"
    C_FN = "clear_stable_prefixes"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
