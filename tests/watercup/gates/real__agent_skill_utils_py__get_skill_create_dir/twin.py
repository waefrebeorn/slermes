"""twin for agent/skill_utils.py:get_skill_create_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_utils.py"
    PY_FN = "get_skill_create_dir"
    C_FN = "get_skill_create_dir"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
