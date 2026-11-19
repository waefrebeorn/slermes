"""twin for agent/battery.py:_read_battery_uncached (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/battery.py"
    PY_FN = "_read_battery_uncached"
    C_FN = "agent_battery_u_read_battery_uncached"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
