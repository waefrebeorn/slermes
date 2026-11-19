"""twin for hermes_cli/config.py:_cron_model_drift_axis_for_config_key (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/config.py"
    PY_FN = "_cron_model_drift_axis_for_config_key"
    C_FN = "_cron_model_drift_axis_for_config_key"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
