"""twin for gateway/platforms/whatsapp_common.py:_is_broadcast_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_common.py"
    PY_FN = "_is_broadcast_chat"
    C_FN = "whatsapp_common_is_broadcast_chat"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
