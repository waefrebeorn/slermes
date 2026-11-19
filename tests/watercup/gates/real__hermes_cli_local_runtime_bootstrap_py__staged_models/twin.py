"""twin for hermes_cli/local_runtime/bootstrap.py:staged_models (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/bootstrap.py"
    PY_FN = "staged_models"
    C_FN = "staged_models"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
