"""twin for hermes_cli/models.py:github_model_reasoning_efforts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/models.py"
    PY_FN = "github_model_reasoning_efforts"
    C_FN = "github_model_reasoning_efforts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
