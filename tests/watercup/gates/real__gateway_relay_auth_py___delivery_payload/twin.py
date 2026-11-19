"""twin for gateway/relay/auth.py:_delivery_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/auth.py"
    PY_FN = "_delivery_payload"
    C_FN = "relay_auth_delivery_payload"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
