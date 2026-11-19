"""twin for tools/thread_context.py:propagate_context_to_thread (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/thread_context.py"
    PY_FN = "propagate_context_to_thread"
    C_FN = "tools_thread_context_propagate_context_to_thread"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
