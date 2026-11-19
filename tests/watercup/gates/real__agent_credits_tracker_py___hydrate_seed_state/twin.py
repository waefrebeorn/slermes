"""twin for agent/credits_tracker.py:_hydrate_seed_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credits_tracker.py"
    PY_FN = "_hydrate_seed_state"
    C_FN = "crt_hydrate_seed_state"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
