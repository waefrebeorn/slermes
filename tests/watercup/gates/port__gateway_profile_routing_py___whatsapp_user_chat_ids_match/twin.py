"""twin for gateway/profile_routing.py:_whatsapp_user_chat_ids_match (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/profile_routing.py"
    PY_FN = "_whatsapp_user_chat_ids_match"
    C_FN = "_whatsapp_user_chat_ids_match"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
