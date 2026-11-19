"""twin for agent/skill_commands.py:scan_skill_commands (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "scan_skill_commands"
    C_FN = "scan_skill_commands"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
