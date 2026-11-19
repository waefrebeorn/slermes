"""twin for gateway/platforms/msgraph_webhook.py:_remember_receipt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "_remember_receipt"
    C_FN = "msgraph_remember_receipt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
