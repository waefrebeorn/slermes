"""twin for hermes_cli/update_cmd.py:_upgrade_pip_before_lazy_refresh (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_upgrade_pip_before_lazy_refresh"
    C_FN = "uc_upgrade_pip_before_lazy_refresh"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
