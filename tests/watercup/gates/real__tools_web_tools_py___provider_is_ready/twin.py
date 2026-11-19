"""twin for tools/web_tools.py:_provider_is_ready (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/web_tools.py"
    PY_FN = "_provider_is_ready"
    C_FN = "_provider_is_ready"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
