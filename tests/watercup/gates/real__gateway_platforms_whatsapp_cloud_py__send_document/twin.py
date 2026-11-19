"""twin for gateway/platforms/whatsapp_cloud.py:send_document (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "send_document"
    C_FN = "send_document"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
