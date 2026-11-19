"""twin for agent/moonshot_schema.py:_fill_missing_type (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moonshot_schema.py"
    PY_FN = "_fill_missing_type"
    C_FN = "_fill_missing_type"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
