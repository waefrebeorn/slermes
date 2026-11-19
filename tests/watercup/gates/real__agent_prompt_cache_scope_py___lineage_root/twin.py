"""twin for agent/prompt_cache_scope.py:_lineage_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_cache_scope.py"
    PY_FN = "_lineage_root"
    C_FN = "_lineage_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
