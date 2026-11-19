"""twin for hermes_cli/cron.py:_scripts_dir_for_cron (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/cron.py"
    PY_FN = "_scripts_dir_for_cron"
    C_FN = "_scripts_dir_for_cron"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
