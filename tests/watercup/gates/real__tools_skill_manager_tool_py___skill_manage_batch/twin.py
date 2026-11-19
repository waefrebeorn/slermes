"""twin for tools/skill_manager_tool.py:_skill_manage_batch (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_manager_tool.py"
    PY_FN = "_skill_manage_batch"
    C_FN = "_skill_manage_batch"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
