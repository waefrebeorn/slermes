"""twin for gateway/platforms/webhook.py:_is_webhook_silence_response (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/webhook.py"
    PY_FN = "_is_webhook_silence_response"
    C_FN = "wh_is_webhook_silence_response"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
