"""twin for tools/homeassistant_tool.py:_check_ha_available (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/homeassistant_tool.py"
    PY_FN = "_check_ha_available"
    C_FN = "tools_homeassistant_tool_u_check_ha_available"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
