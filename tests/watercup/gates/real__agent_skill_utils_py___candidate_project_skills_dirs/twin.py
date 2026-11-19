"""twin for agent/skill_utils.py:_candidate_project_skills_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "_candidate_project_skills_dirs"
    C_FN = "_candidate_project_skills_dirs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
