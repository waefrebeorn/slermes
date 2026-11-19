"""twin for gateway/relay/transport.py:set_passthrough_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/transport.py"
    PY_FN = "set_passthrough_handler"
    C_FN = "relay_transport_set_passthrough_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
