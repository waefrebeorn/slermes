"""twin for cron/suggestions.py:_secure_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/suggestions.py"
    PY_FN = "_secure_file"
    C_FN = "cron_sugg_secure_file"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
