"""twin for gateway/relay/adapter.py:_with_format_hints_for_platform (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/relay/adapter.py"
    PY_FN = "_with_format_hints_for_platform"
    C_FN = "_with_format_hints_for_platform"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
