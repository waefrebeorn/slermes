"""twin for agent/replay_cleanup.py:sanitize_replay_history (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/replay_cleanup.py"
    PY_FN = "sanitize_replay_history"
    C_FN = "sanitize_replay_history"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
