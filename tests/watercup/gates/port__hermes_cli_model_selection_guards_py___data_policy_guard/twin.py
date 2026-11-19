"""twin for hermes_cli/model_selection_guards.py:_data_policy_guard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/model_selection_guards.py"
    PY_FN = "_data_policy_guard"
    C_FN = "_data_policy_guard"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
