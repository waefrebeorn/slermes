"""twin for gateway/platforms/msgraph_webhook.py:_has_seen_receipt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "_has_seen_receipt"
    C_FN = "msgraph_has_seen_receipt"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
