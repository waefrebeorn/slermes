"""twin for agent/moonshot_schema.py:is_moonshot_model (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moonshot_schema.py"
    PY_FN = "is_moonshot_model"
    C_FN = "msc_is_moonshot_model"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
