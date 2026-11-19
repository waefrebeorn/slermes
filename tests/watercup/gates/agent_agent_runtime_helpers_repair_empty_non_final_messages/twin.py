"""twin for agent/agent_runtime_helpers.py:repair_empty_non_final_messages (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "repair_empty_non_final_messages"
    C_FN = "repair_empty_non_final_messages"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
