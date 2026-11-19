"""twin for hermes_cli/local_runtime/estimator.py:kv_dtype_factor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/estimator.py"
    PY_FN = "kv_dtype_factor"
    C_FN = "kv_dtype_factor"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
