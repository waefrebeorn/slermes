"""twin for gateway/session.py:_append_to_transcript_serialized (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/session.py"
    PY_FN = "_append_to_transcript_serialized"
    C_FN = "_append_to_transcript_serialized"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
