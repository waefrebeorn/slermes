"""twin for hermes_cli/managed_scope.py:get_managed_dir (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_scope.py"
    PY_FN = "get_managed_dir"
    C_FN = "hermes_cli_managed_scope_get_managed_dir"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
