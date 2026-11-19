"""twin for cron/scheduler.py:_is_transient_provider_resolve_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler.py"
    PY_FN = "_is_transient_provider_resolve_error"
    C_FN = "_is_transient_provider_resolve_error"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
