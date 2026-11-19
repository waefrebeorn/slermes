"""twin for agent/conversation_compression.py:_install_compression_cancelled_check (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/conversation_compression.py"
    PY_FN = "_install_compression_cancelled_check"
    C_FN = "_install_compression_cancelled_check"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
