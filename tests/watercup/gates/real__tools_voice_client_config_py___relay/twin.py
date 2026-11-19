"""twin for tools/voice_client_config.py:_relay (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_client_config.py"
    PY_FN = "_relay"
    C_FN = "_relay"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
