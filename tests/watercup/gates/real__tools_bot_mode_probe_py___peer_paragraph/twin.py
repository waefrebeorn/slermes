"""twin for tools/bot_mode_probe.py:_peer_paragraph (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_probe.py"
    PY_FN = "_peer_paragraph"
    C_FN = "bmp_peer_paragraph"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
