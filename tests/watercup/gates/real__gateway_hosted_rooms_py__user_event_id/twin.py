"""twin for gateway/hosted_rooms.py:user_event_id (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "user_event_id"
    C_FN = "user_event_id"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
