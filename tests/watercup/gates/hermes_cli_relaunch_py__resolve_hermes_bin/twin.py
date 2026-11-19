"""twin for hermes_cli/relaunch.py:resolve_hermes_bin (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/relaunch.py"
    PY_FN = "resolve_hermes_bin"
    C_FN = "hermes_cli_relaunch_resolve_hermes_bin"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
