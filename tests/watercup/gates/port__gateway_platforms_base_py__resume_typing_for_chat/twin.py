"""twin for gateway/platforms/base.py:resume_typing_for_chat (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "resume_typing_for_chat"
    C_FN = "gw_base__resume_typing_for_chat"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
