"""twin for hermes_cli/sqlite_safe_read.py:page_count_bytes (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_safe_read.py"
    PY_FN = "page_count_bytes"
    C_FN = "sqlite_safe_page_count_bytes"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
