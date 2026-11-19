"""twin for hermes_cli/managed_scope.py:_flatten_keys (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_scope.py"
    PY_FN = "_flatten_keys"
    C_FN = "_flatten_keys"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
