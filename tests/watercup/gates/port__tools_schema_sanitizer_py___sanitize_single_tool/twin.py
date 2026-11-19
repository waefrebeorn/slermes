"""twin for tools/schema_sanitizer.py:_sanitize_single_tool (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/schema_sanitizer.py"
    PY_FN = "_sanitize_single_tool"
    C_FN = "_sanitize_single_tool"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
