"""twin for gateway/hosted_rooms.py:probe_peer_room_reservation (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_rooms.py"
    PY_FN = "probe_peer_room_reservation"
    C_FN = "probe_peer_room_reservation"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
