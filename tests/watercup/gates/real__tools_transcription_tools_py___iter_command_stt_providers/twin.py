"""twin for tools/transcription_tools.py:_iter_command_stt_providers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_iter_command_stt_providers"
    C_FN = "_iter_command_stt_providers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
