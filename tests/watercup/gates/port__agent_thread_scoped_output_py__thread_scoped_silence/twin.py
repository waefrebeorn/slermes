"""twin for agent/thread_scoped_output.py:thread_scoped_silence (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/thread_scoped_output.py"
    PY_FN = "thread_scoped_silence"
    C_FN = "agent_thread_scoped_output_thread_scoped_silence"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
