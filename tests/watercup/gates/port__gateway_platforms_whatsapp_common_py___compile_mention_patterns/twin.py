"""twin for gateway/platforms/whatsapp_common.py:_compile_mention_patterns (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/platforms/whatsapp_common.py"
    PY_FN = "_compile_mention_patterns"
    C_FN = "_compile_mention_patterns"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
