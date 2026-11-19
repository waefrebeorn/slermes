"""twin for hermes_cli/dashboard_procs.py:_valid_lockfile_payload (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_procs.py"
    PY_FN = "_valid_lockfile_payload"
    C_FN = "dashboard_procs_valid_lockfile_payload"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
