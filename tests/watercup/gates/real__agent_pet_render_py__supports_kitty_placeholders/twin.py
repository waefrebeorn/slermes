"""twin for agent/pet/render.py:supports_kitty_placeholders (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/pet/render.py"
    PY_FN = "supports_kitty_placeholders"
    C_FN = "supports_kitty_placeholders"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
