"""twin for gateway/hosted_rooms.py:probe_hosted_room (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "probe_hosted_room"
    C_FN = "probe_hosted_room"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
