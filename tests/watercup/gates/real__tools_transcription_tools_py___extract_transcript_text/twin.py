"""twin for tools/transcription_tools.py:_extract_transcript_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/transcription_tools.py"
    PY_FN = "_extract_transcript_text"
    C_FN = "_extract_transcript_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
