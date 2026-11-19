"""twin for gateway/platforms/webhook.py:_handle_health (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_handle_health"
    C_FN = "_handle_health"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
