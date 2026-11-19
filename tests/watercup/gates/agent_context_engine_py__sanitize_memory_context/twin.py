"""twin for agent/context_engine.py:sanitize_memory_context (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_engine.py"
    PY_FN = "sanitize_memory_context"
    C_FN = "agent_context_engine_sanitize_memory_context"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
