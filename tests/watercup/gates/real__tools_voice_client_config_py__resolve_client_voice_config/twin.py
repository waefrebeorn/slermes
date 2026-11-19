"""twin for tools/voice_client_config.py:resolve_client_voice_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_client_config.py"
    PY_FN = "resolve_client_voice_config"
    C_FN = "resolve_client_voice_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
