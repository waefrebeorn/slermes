"""twin for gateway/hosted_room_peer.py:room_grant_needs_dispatch_refresh (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_peer.py"
    PY_FN = "room_grant_needs_dispatch_refresh"
    C_FN = "room_grant_needs_dispatch_refresh"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
