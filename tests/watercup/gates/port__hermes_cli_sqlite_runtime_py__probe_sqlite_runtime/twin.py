"""twin for hermes_cli/sqlite_runtime.py:probe_sqlite_runtime (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_runtime.py"
    PY_FN = "probe_sqlite_runtime"
    C_FN = "hermes_cli_sqlite_runtime_probe_sqlite_runtime"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
