"""twin for agent/skill_utils.py:get_untrusted_project_skills_root (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "get_untrusted_project_skills_root"
    C_FN = "get_untrusted_project_skills_root"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
