"""twin for tools/environments/daytona.py:_daytona_upload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/daytona.py"
    PY_FN = "_daytona_upload"
    C_FN = "cli_tools_environments_daytona__daytona_upload"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
