"""twin for gateway/run.py:_track_deferred_agent_worker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/run.py"
    PY_FN = "_track_deferred_agent_worker"
    C_FN = "_track_deferred_agent_worker"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
