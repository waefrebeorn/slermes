"""twin for gateway/platforms/webhook.py:_end_webhook_session (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_end_webhook_session"
    C_FN = "DB"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
