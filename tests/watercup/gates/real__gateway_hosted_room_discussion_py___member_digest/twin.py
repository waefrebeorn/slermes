"""twin for gateway/hosted_room_discussion.py:_member_digest (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_discussion.py"
    PY_FN = "_member_digest"
    C_FN = "_member_digest"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
