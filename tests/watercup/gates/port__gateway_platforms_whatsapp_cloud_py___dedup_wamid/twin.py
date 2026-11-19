"""twin for gateway/platforms/whatsapp_cloud.py:_dedup_wamid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "_dedup_wamid"
    C_FN = "whatsapp_cloud_dedup_wamid"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
