"""twin for hermes_cli/session_lost_and_found.py:rebuild_fts_indexes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_lost_and_found.py"
    PY_FN = "rebuild_fts_indexes"
    C_FN = "rebuild_fts_indexes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
