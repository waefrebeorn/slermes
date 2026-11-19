"""twin for gateway/hosted_room_policy_checkpoint.py:events_for_task (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_policy_checkpoint.py"
    PY_FN = "events_for_task"
    C_FN = "events_for_task"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
