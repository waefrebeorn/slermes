"""twin for cli.py:_voice_submit_barge_utterance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_voice_submit_barge_utterance"
    C_FN = "_voice_submit_barge_utterance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
