"""twin for tools/delegation_live_log.py:prune_stale_live_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/delegation_live_log.py"
    PY_FN = "prune_stale_live_dirs"
    C_FN = "tools_delegation_live_log_prune_stale_live_dirs"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
