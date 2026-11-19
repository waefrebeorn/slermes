"""twin for agent/agent_runtime_helpers.py:drop_thinking_only_and_merge_users (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_runtime_helpers.py"
    PY_FN = "drop_thinking_only_and_merge_users"
    C_FN = "drop_thinking_only_and_merge_users"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
