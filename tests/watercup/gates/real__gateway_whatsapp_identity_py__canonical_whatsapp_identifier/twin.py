"""twin for gateway/whatsapp_identity.py:canonical_whatsapp_identifier (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/whatsapp_identity.py"
    PY_FN = "canonical_whatsapp_identifier"
    C_FN = "canonical_whatsapp_identifier"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
