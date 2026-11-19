"""twin for agent/skill_commands.py:build_stacked_skill_invocation_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/skill_commands.py"
    PY_FN = "build_stacked_skill_invocation_message"
    C_FN = "agent_skill_commands_build_stacked_skill_invocation_message"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
