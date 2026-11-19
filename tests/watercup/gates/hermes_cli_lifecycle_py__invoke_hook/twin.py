"""twin for hermes_cli/lifecycle.py:invoke_hook (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/lifecycle.py"
    PY_FN = "invoke_hook"
    C_FN = "hermes_cli_lifecycle_invoke_hook"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
