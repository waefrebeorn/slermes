"""twin for hermes_cli/local_runtime/supervisor.py:model_failures (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/supervisor.py"
    PY_FN = "model_failures"
    C_FN = "model_failures"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
