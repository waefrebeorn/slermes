"""twin for agent/skill_commands.py:_inject_skill_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "_inject_skill_config"
    C_FN = "_inject_skill_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
