"""twin for agent/skill_utils.py:iter_skill_index_files (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "iter_skill_index_files"
    C_FN = "iter_skill_index_files"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
