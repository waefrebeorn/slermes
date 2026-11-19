"""twin for gateway/relay/transport.py:send_follow_up (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/transport.py"
    PY_FN = "send_follow_up"
    C_FN = "send_follow_up"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
