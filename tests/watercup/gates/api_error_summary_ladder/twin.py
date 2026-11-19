"""twin for run_agent.py:_summarize_api_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "run_agent.py"
    PY_FN = "_summarize_api_error"
    C_FN = "_summarize_api_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
