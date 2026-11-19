"""twin for gateway/platforms/msgraph_webhook.py:_build_message_event (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "_build_message_event"
    C_FN = "_build_message_event"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
