"""twin for hermes_cli/profile_distribution.py:_has_cron_jobs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/profile_distribution.py"
    PY_FN = "_has_cron_jobs"
    C_FN = "hermes_cli_profile_distributio_u_has_cron_jobs"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
