"""twin for gateway/profile_routing.py:_is_whatsapp_non_user_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/profile_routing.py"
    PY_FN = "_is_whatsapp_non_user_chat"
    C_FN = "_is_whatsapp_non_user_chat"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
