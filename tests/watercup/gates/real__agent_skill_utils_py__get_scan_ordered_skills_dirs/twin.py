"""twin for agent/skill_utils.py:get_scan_ordered_skills_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "get_scan_ordered_skills_dirs"
    C_FN = "get_scan_ordered_skills_dirs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
