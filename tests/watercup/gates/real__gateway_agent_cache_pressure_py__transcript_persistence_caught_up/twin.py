"""twin for gateway/agent_cache_pressure.py:transcript_persistence_caught_up (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/agent_cache_pressure.py"
    PY_FN = "transcript_persistence_caught_up"
    C_FN = "acb_transcript_persistence_caught_up"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
