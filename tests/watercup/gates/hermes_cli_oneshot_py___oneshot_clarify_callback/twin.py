"""twin for hermes_cli/oneshot.py:_oneshot_clarify_callback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/oneshot.py"
    PY_FN = "_oneshot_clarify_callback"
    C_FN = "hermes_cli_oneshot_u_oneshot_clarify_callback"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
