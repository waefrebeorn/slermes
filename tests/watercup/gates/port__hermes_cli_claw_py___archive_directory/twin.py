"""twin for hermes_cli/claw.py:_archive_directory (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/claw.py"
    PY_FN = "_archive_directory"
    C_FN = "hermes_cli_claw_u_archive_directory"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
