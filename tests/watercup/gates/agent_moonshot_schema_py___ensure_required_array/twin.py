"""twin for agent/moonshot_schema.py:_ensure_required_array (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moonshot_schema.py"
    PY_FN = "_ensure_required_array"
    C_FN = "agent_moonshot_schema_u_ensure_required_array"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
