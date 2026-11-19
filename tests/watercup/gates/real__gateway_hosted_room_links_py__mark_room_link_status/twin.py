"""twin for gateway/hosted_room_links.py:mark_room_link_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_links.py"
    PY_FN = "mark_room_link_status"
    C_FN = "mark_room_link_status"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
