"""twin for tools/tool_backend_helpers.py:read_selection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_backend_helpers.py"
    PY_FN = "read_selection"
    C_FN = "read_selection"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
