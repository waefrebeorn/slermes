"""twin for agent/pet/render.py:kitty_color_hex (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/render.py"
    PY_FN = "kitty_color_hex"
    C_FN = "kitty_color_hex"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
