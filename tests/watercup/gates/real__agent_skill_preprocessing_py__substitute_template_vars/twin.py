"""twin for agent/skill_preprocessing.py:substitute_template_vars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_preprocessing.py"
    PY_FN = "substitute_template_vars"
    C_FN = "substitute_template_vars"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
