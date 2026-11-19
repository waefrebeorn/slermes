"""twin for hermes_cli/sqlite_util.py:add_column_if_missing (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_util.py"
    PY_FN = "add_column_if_missing"
    C_FN = "sqlite_util_add_column_if_missing"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
