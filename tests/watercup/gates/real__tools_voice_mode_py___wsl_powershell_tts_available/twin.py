"""twin for tools/voice_mode.py:_wsl_powershell_tts_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/voice_mode.py"
    PY_FN = "_wsl_powershell_tts_available"
    C_FN = "_wsl_powershell_tts_available"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
