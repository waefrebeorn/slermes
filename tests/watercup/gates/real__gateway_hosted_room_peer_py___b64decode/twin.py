"""twin for gateway/hosted_room_peer.py:_b64decode (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_peer.py"
    PY_FN = "_b64decode"
    C_FN = "_b64decode"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
