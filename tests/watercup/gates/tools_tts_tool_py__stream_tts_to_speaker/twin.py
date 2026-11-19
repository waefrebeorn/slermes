"""twin for tools/tts_tool.py:stream_tts_to_speaker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "stream_tts_to_speaker"
    C_FN = "tts_tool_stream_tts_to_speaker"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
