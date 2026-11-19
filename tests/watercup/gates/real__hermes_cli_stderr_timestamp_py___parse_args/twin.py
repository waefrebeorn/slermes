"""twin for hermes_cli/stderr_timestamp.py:_parse_args (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/stderr_timestamp.py"
    PY_FN = "_parse_args"
    C_FN = "_parse_args"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
