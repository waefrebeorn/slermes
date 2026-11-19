"""twin for gateway/platforms/whatsapp_cloud.py:check_whatsapp_cloud_requirements (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "check_whatsapp_cloud_requirements"
    C_FN = "whatsapp_cloud_check_requirements"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
