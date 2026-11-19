"""twin for hermes_cli/foreign_sessions.py:_read_json_lines (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/foreign_sessions.py"
    PY_FN = "_read_json_lines"
    C_FN = "_read_json_lines"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
