"""twin for hermes_cli/local_runtime/context_policy.py:ladder (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/context_policy.py"
    PY_FN = "ladder"
    C_FN = "ladder"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
