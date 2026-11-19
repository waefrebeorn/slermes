"""twin for agent/tts_provider.py:synthesize (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/tts_provider.py"
    PY_FN = "synthesize"
    C_FN = "synthesize"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
