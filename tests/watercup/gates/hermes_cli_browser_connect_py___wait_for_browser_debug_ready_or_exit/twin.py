"""twin for hermes_cli/browser_connect.py:_wait_for_browser_debug_ready_or_exit (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/browser_connect.py"
    PY_FN = "_wait_for_browser_debug_ready_or_exit"
    C_FN = "hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
