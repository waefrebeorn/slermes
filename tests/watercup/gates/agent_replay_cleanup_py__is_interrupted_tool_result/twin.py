"""twin for agent/replay_cleanup.py:is_interrupted_tool_result (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/replay_cleanup.py"
    PY_FN = "is_interrupted_tool_result"
    C_FN = "agent_replay_cleanup_is_interrupted_tool_result"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
