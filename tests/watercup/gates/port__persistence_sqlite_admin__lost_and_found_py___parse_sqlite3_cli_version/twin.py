"""twin for hermes_cli/session_lost_and_found.py:_parse_sqlite3_cli_version (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "_parse_sqlite3_cli_version"
    C_FN = "_parse_sqlite3_cli_version"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
