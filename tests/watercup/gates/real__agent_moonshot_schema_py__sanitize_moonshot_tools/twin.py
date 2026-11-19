"""twin for agent/moonshot_schema.py:sanitize_moonshot_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moonshot_schema.py"
    PY_FN = "sanitize_moonshot_tools"
    C_FN = "sanitize_moonshot_tools"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
