"""twin for agent/skill_commands.py:extract_user_instruction_from_skill_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "extract_user_instruction_from_skill_message"
    C_FN = "extract_user_instruction_from_skill_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
