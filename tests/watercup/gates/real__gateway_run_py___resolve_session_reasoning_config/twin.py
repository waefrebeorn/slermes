"""twin for gateway/run.py:_resolve_session_reasoning_config (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_resolve_session_reasoning_config"
    C_FN = "_resolve_session_reasoning_config"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
