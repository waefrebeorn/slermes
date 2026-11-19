"""twin for hermes_cli/main.py:_try_redownload_electron_dist (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_try_redownload_electron_dist"
    C_FN = "mn_try_redownload_electron_dist"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
