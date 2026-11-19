"""twin for gateway/hosted_room_discussion.py:_format_message (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_discussion.py"
    PY_FN = "_format_message"
    C_FN = "_format_message"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
