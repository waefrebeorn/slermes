"""twin for gateway/hosted_room_peer.py:local_room_link_endpoint (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_peer.py"
    PY_FN = "local_room_link_endpoint"
    C_FN = "local_room_link_endpoint"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
