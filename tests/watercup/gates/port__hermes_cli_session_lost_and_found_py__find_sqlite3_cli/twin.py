"""twin for hermes_cli/session_lost_and_found.py:find_sqlite3_cli (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "find_sqlite3_cli"
    C_FN = "find_sqlite3_cli"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
