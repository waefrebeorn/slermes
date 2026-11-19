"""twin for gateway/run.py:_should_echo_stt_transcripts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_should_echo_stt_transcripts"
    C_FN = "_should_echo_stt_transcripts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
