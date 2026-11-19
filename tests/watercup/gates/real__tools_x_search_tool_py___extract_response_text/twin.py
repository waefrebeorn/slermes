"""twin for tools/x_search_tool.py:_extract_response_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/x_search_tool.py"
    PY_FN = "_extract_response_text"
    C_FN = "_extract_response_text"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
