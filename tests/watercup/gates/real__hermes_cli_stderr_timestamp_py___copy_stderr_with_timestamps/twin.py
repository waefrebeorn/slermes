"""twin for hermes_cli/stderr_timestamp.py:_copy_stderr_with_timestamps (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"
    PY_FN = "_copy_stderr_with_timestamps"
    C_FN = "_copy_stderr_with_timestamps"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
