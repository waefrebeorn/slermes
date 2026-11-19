"""twin for gateway/browser_control_broker.py:consume_ticket (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/browser_control_broker.py"
    PY_FN = "consume_ticket"
    C_FN = "bcb_broker_consume_ticket"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
