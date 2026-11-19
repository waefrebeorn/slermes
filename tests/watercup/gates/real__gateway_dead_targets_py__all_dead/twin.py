"""twin for gateway/dead_targets.py:all_dead (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/dead_targets.py"
    PY_FN = "all_dead"
    C_FN = "all_dead"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
