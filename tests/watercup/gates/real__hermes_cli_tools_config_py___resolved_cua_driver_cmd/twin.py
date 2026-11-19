"""twin for hermes_cli/tools_config.py:_resolved_cua_driver_cmd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/tools_config.py"
    PY_FN = "_resolved_cua_driver_cmd"
    C_FN = "_resolved_cua_driver_cmd"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
