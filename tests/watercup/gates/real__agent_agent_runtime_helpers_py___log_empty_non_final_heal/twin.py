"""twin for agent/agent_runtime_helpers.py:_log_empty_non_final_heal (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "_log_empty_non_final_heal"
    C_FN = "_log_empty_non_final_heal"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
