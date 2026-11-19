"""twin for gateway/platforms/msgraph_webhook.py:_normalize_resource_value (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/msgraph_webhook.py"
    PY_FN = "_normalize_resource_value"
    C_FN = "_normalize_resource_value"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
