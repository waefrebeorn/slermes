"""twin for hermes_state.py:_watched_sqlite_sidecar_paths (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_state.py"
    PY_FN = "_watched_sqlite_sidecar_paths"
    C_FN = "_watched_sqlite_sidecar_paths"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
