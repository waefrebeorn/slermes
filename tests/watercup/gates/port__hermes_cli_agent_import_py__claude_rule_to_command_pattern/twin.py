"""twin for hermes_cli/agent_import.py:claude_rule_to_command_pattern (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/agent_import.py"
    PY_FN = "claude_rule_to_command_pattern"
    C_FN = "ai_claude_rule_to_command_pattern"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
