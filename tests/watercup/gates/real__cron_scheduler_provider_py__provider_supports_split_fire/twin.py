"""twin for cron/scheduler_provider.py:provider_supports_split_fire (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/scheduler_provider.py"
    PY_FN = "provider_supports_split_fire"
    C_FN = "provider_supports_split_fire"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
