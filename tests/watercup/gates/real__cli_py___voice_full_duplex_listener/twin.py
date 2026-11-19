"""twin for cli.py:_voice_full_duplex_listener (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_voice_full_duplex_listener"
    C_FN = "_voice_full_duplex_listener"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
