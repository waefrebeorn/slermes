"""twin for agent/skill_preprocessing.py:load_skills_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_preprocessing.py"
    PY_FN = "load_skills_config"
    C_FN = "load_skills_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
