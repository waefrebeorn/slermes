"""twin for hermes_cli/update_cmd.py:_purge_stale_hermes_modules (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_purge_stale_hermes_modules"
    C_FN = "uc_purge_stale_hermes_modules"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
