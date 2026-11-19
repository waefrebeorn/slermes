"""twin for tools/tool_search.py:estimate_tokens_from_schemas (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tool_search.py"
    PY_FN = "estimate_tokens_from_schemas"
    C_FN = "ts_estimate_tokens_from_schemas"

    def norm(self, c, result):
        c.valstr(str(int(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
