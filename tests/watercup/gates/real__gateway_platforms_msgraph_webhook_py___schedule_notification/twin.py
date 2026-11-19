"""twin for gateway/platforms/msgraph_webhook.py:_schedule_notification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "_schedule_notification"
    C_FN = "msgraph_schedule_notification"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
