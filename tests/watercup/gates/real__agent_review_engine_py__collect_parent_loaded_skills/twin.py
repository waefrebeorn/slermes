"""twin for agent/review_engine.py:collect_parent_loaded_skills (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/review_engine.py"
    PY_FN = "collect_parent_loaded_skills"
    C_FN = "collect_parent_loaded_skills"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
