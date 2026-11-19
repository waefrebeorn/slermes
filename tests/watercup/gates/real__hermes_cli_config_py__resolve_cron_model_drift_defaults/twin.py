"""twin for hermes_cli/config.py:resolve_cron_model_drift_defaults (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "resolve_cron_model_drift_defaults"
    C_FN = "resolve_cron_model_drift_defaults"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
