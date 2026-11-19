"""twin for tools/skill_manager_tool.py:_resolve_skill_target (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_manager_tool.py"
    PY_FN = "_resolve_skill_target"
    C_FN = "_resolve_skill_target"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
