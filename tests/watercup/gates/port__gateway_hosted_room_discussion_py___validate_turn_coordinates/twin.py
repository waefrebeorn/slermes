"""twin for gateway/hosted_room_discussion.py:_validate_turn_coordinates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/hosted_room_discussion.py"
    PY_FN = "_validate_turn_coordinates"
    C_FN = "_validate_turn_coordinates"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
