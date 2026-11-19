"""twin for gateway/relay/ws_transport.py:set_interrupt_inbound_handler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/ws_transport.py"
    PY_FN = "set_interrupt_inbound_handler"
    C_FN = "ws_transport_set_interrupt_inbound_handler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
