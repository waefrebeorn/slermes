"""twin for hermes_cli/sqlite_util.py:write_txn (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_util.py"
    PY_FN = "write_txn"
    C_FN = "sqlite_util_write_txn_begin"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
