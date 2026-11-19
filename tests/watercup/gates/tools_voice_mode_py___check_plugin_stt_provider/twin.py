"""twin for tools/voice_mode.py:_check_plugin_stt_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "_check_plugin_stt_provider"
    C_FN = "voice_check_plugin_stt_provider"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
