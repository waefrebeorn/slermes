"""twin for agent/thread_scoped_output.py:unsilence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/thread_scoped_output.py"
    PY_FN = "unsilence"
    C_FN = "agent_thread_scoped_output_unsilence"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
