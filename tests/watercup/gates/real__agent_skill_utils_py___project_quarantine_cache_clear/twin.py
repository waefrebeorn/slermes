"""twin for agent/skill_utils.py:_project_quarantine_cache_clear (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "_project_quarantine_cache_clear"
    C_FN = "_project_quarantine_cache_clear"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
