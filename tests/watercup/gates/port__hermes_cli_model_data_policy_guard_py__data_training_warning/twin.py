"""twin for hermes_cli/model_data_policy_guard.py:data_training_warning (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_data_policy_guard.py"
    PY_FN = "data_training_warning"
    C_FN = "data_training_warning"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
