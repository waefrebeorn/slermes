"""twin for gateway/pairing.py:_platform_uses_whatsapp_identity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/pairing.py"
    PY_FN = "_platform_uses_whatsapp_identity"
    C_FN = "platform_uses_whatsapp_identity"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
