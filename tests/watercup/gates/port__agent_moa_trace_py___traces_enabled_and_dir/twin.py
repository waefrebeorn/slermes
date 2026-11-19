"""twin for agent/moa_trace.py:_traces_enabled_and_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_trace.py"
    PY_FN = "_traces_enabled_and_dir"
    C_FN = "agent_moa_trace_u_traces_enabled_and_dir"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
