"""twin for agent/moa_loop.py:_maybe_apply_moa_cache_control (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/moa_loop.py"
    PY_FN = "_maybe_apply_moa_cache_control"
    C_FN = "moa_u_maybe_apply_moa_cache_control"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
