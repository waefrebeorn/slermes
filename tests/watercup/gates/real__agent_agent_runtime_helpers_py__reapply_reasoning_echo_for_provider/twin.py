"""twin for agent/agent_runtime_helpers.py:reapply_reasoning_echo_for_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "reapply_reasoning_echo_for_provider"
    C_FN = "agent_runtime_helpers_reapply_reasoning_echo_for_provider"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
