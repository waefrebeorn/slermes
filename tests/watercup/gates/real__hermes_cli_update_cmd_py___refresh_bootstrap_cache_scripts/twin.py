"""twin for hermes_cli/update_cmd.py:_refresh_bootstrap_cache_scripts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_refresh_bootstrap_cache_scripts"
    C_FN = "update_refresh_bootstrap_cache_scripts"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
