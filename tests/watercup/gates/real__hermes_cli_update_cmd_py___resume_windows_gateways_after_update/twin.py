"""twin for hermes_cli/update_cmd.py:_resume_windows_gateways_after_update (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_cmd.py"
    PY_FN = "_resume_windows_gateways_after_update"
    C_FN = "_resume_windows_gateways_after_update"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
