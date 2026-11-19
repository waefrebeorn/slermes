"""twin for hermes_cli/managed_scope.py:_parse_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_scope.py"
    PY_FN = "_parse_env"
    C_FN = "_parse_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
