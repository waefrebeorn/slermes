"""twin for hermes_cli/stderr_timestamp.py:_restore_signal_handlers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"
    PY_FN = "_restore_signal_handlers"
    C_FN = "hermes_cli_stderr_timestamp_restore_signal_handlers"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
