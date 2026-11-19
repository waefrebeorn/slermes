"""twin for tools/browser_lightpanda.py:find_lightpanda_binary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_lightpanda.py"
    PY_FN = "find_lightpanda_binary"
    C_FN = "find_lightpanda_binary"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
