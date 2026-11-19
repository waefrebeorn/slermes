"""twin for agent/skill_preprocessing.py:preprocess_skill_content (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_preprocessing.py"
    PY_FN = "preprocess_skill_content"
    C_FN = "preprocess_skill_content"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
