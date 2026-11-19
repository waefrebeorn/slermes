"""twin for gateway/hosted_room_policy_checkpoint.py:_store_transcript_event (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_policy_checkpoint.py"
    PY_FN = "_store_transcript_event"
    C_FN = "_store_transcript_event"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
