"""twin for hermes_cli/sqlite_safe_read.py:read_header_bytes_preopen (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_safe_read.py"
    PY_FN = "read_header_bytes_preopen"
    C_FN = "read_header_bytes_preopen"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
