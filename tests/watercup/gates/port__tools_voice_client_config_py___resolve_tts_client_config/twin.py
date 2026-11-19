"""twin for tools/voice_client_config.py:_resolve_tts_client_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_client_config.py"
    PY_FN = "_resolve_tts_client_config"
    C_FN = "_resolve_tts_client_config"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
