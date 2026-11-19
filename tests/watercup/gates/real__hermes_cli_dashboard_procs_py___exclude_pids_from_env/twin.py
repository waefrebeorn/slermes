"""twin for hermes_cli/dashboard_procs.py:_exclude_pids_from_env (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/dashboard_procs.py"
    PY_FN = "_exclude_pids_from_env"
    C_FN = "_exclude_pids_from_env"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
