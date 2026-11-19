"""twin for tools/transcription_tools.py:_quote_command_stt_placeholder (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_quote_command_stt_placeholder"
    C_FN = "_quote_command_stt_placeholder"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
