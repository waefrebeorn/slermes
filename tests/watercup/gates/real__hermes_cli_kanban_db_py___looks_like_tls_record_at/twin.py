"""twin for hermes_cli/kanban_db.py:_looks_like_tls_record_at (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/kanban_db.py"
    PY_FN = "_looks_like_tls_record_at"
    C_FN = "_looks_like_tls_record_at"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
