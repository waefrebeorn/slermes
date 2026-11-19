"""twin for tools/voice_client_config.py:_client_direct_enabled (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_client_config.py"
    PY_FN = "_client_direct_enabled"
    C_FN = "_client_direct_enabled"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
