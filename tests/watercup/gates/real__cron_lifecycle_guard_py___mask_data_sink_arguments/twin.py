"""twin for cron/lifecycle_guard.py:_mask_data_sink_arguments (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/lifecycle_guard.py"
    PY_FN = "_mask_data_sink_arguments"
    C_FN = "_mask_data_sink_arguments"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
