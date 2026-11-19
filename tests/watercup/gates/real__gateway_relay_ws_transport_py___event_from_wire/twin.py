"""twin for gateway/relay/ws_transport.py:_event_from_wire (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/ws_transport.py"
    PY_FN = "_event_from_wire"
    C_FN = "_event_from_wire"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
