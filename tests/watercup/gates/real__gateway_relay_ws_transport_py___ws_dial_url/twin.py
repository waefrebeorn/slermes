"""twin for gateway/relay/ws_transport.py:_ws_dial_url (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/ws_transport.py"
    PY_FN = "_ws_dial_url"
    C_FN = "_ws_dial_url"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
