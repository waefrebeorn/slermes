"""twin for gateway/whatsapp_identity.py:expand_whatsapp_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/whatsapp_identity.py"
    PY_FN = "expand_whatsapp_aliases"
    C_FN = "expand_whatsapp_aliases"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
