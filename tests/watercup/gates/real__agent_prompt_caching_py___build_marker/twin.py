"""twin for agent/prompt_caching.py:_build_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/prompt_caching.py"
    PY_FN = "_build_marker"
    C_FN = "_build_marker"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
