"""twin for tools/tool_search.py:_describe_classification (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_search.py"
    PY_FN = "_describe_classification"
    C_FN = "_describe_classification"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
