"""twin for tools/transcription_tools.py:_is_command_stt_provider_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_is_command_stt_provider_config"
    C_FN = "tsc_is_command_stt_provider_config"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
