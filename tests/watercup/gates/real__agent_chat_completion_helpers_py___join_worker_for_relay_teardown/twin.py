"""twin for agent/chat_completion_helpers.py:_join_worker_for_relay_teardown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/chat_completion_helpers.py"
    PY_FN = "_join_worker_for_relay_teardown"
    C_FN = "_join_worker_for_relay_teardown"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
