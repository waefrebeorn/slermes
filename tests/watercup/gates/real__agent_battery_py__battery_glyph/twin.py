"""twin for agent/battery.py:battery_glyph (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/battery.py"
    PY_FN = "battery_glyph"
    C_FN = "battery_glyph"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
