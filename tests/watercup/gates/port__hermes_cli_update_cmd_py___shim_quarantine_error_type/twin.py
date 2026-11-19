"""twin for hermes_cli/update_cmd.py:_shim_quarantine_error_type (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_shim_quarantine_error_type"
    C_FN = "uc_shim_quarantine_error_type"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
