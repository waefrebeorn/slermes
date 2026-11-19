"""twin for gateway/platforms/msgraph_webhook.py:set_notification_scheduler (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "set_notification_scheduler"
    C_FN = "msgraph_webhook_set_scheduler"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
