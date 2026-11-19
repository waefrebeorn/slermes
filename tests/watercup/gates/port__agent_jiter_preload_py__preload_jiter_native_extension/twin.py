"""twin for agent/jiter_preload.py:preload_jiter_native_extension (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/jiter_preload.py"
    PY_FN = "preload_jiter_native_extension"
    C_FN = "agent_jiter_preload_preload_jiter_native_extension"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
