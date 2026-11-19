"""twin for agent/skill_commands.py:resolve_skill_command_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "resolve_skill_command_key"
    C_FN = "resolve_skill_command_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
