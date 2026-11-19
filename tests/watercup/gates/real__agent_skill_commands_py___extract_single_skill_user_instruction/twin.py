"""twin for agent/skill_commands.py:_extract_single_skill_user_instruction (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "_extract_single_skill_user_instruction"
    C_FN = "_extract_single_skill_user_instruction"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
