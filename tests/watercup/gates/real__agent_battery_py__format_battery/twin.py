"""twin for agent/battery.py:format_battery (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/battery.py"
    PY_FN = "format_battery"
    C_FN = "format_battery"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
