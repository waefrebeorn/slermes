"""twin for cron/executions.py:recover_interrupted_executions (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/executions.py"
    PY_FN = "recover_interrupted_executions"
    C_FN = "cron_executions_recover_interrupted_executions"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
