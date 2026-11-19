"""twin for gateway/hosted_room_links.py:_short_string (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_links.py"
    PY_FN = "_short_string"
    C_FN = "_short_string"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
