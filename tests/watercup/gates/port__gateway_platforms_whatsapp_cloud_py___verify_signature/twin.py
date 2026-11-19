"""twin for gateway/platforms/whatsapp_cloud.py:_verify_signature (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_cloud.py"
    PY_FN = "_verify_signature"
    C_FN = "whatsapp_cloud_verify_signature"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
