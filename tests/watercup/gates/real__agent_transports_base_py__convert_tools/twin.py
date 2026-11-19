"""twin for agent/transports/base.py:convert_tools (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/base.py"
    PY_FN = "convert_tools"
    C_FN = "convert_tools"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
