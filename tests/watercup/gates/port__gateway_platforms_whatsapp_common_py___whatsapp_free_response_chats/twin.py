"""twin for gateway/platforms/whatsapp_common.py:_whatsapp_free_response_chats (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_common.py"
    PY_FN = "_whatsapp_free_response_chats"
    C_FN = "whatsapp_common_free_response_chats"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
