"""twin for agent/pet/render.py:detect_terminal_graphics (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/render.py"
    PY_FN = "detect_terminal_graphics"
    C_FN = "pet_detect_terminal_graphics"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
