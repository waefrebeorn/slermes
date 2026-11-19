"""twin for hermes_cli/kanban_db.py:_scan_prose_for_phantom_ids (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_scan_prose_for_phantom_ids"
    C_FN = "kdb_scan_prose_for_phantom_ids"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
