"""twin for hermes_cli/sqlite_safe_read.py:file_length_matches_header (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/sqlite_safe_read.py"
    PY_FN = "file_length_matches_header"
    C_FN = "sqlite_safe_file_length_matches_header"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
