"""twin for gateway/platforms/base.py:is_chat_level_not_found (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/base.py"
    PY_FN = "is_chat_level_not_found"
    C_FN = "base_platform_is_chat_level_not_found"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
