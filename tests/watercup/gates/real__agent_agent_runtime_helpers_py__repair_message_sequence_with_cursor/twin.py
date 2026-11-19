"""twin for agent/agent_runtime_helpers.py:repair_message_sequence_with_cursor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "repair_message_sequence_with_cursor"
    C_FN = "repair_message_sequence_with_cursor"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
