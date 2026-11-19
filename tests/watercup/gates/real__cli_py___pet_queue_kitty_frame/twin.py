"""twin for cli.py:_pet_queue_kitty_frame (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_pet_queue_kitty_frame"
    C_FN = "_pet_queue_kitty_frame"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
