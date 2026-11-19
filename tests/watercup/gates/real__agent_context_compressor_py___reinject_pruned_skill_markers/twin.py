"""twin for agent/context_compressor.py:_reinject_pruned_skill_markers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_reinject_pruned_skill_markers"
    C_FN = "_reinject_pruned_skill_markers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
