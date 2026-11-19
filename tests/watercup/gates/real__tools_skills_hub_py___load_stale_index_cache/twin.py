"""twin for tools/skills_hub.py:_load_stale_index_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skills_hub.py"
    PY_FN = "_load_stale_index_cache"
    C_FN = "_load_stale_index_cache"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
