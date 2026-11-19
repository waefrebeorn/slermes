"""twin for hermes_cli/update_cmd.py:_clear_stale_sqlite_sidecars (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_clear_stale_sqlite_sidecars"
    C_FN = "_clear_stale_sqlite_sidecars"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
