"""twin for hermes_cli/stderr_timestamp.py:_write_timestamped_line (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"
    PY_FN = "_write_timestamped_line"
    C_FN = "_write_timestamped_line"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
