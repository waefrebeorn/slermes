"""twin for agent/skill_commands.py:_resolve_skill_commands_platform (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "_resolve_skill_commands_platform"
    C_FN = "_resolve_skill_commands_platform"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
