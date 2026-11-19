"""twin for gateway/hosted_room_links.py:make_stored_link (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_links.py"
    PY_FN = "make_stored_link"
    C_FN = "make_stored_link"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
