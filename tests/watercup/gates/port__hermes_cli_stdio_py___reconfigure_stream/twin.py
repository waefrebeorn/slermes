"""twin for hermes_cli/stdio.py:_reconfigure_stream (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stdio.py"
    PY_FN = "_reconfigure_stream"
    C_FN = "hermes_cli_stdio_u_reconfigure_stream"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
