"""twin for tools/delegate_tool.py:_clean_error_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegate_tool.py"
    PY_FN = "_clean_error_text"
    C_FN = "_clean_error_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
