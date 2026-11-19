"""twin for gateway/hosted_room_policy_checkpoint.py:_backfill_transcript (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_policy_checkpoint.py"
    PY_FN = "_backfill_transcript"
    C_FN = "_backfill_transcript"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
