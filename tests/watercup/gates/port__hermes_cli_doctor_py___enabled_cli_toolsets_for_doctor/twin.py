"""twin for hermes_cli/doctor.py:_enabled_cli_toolsets_for_doctor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_enabled_cli_toolsets_for_doctor"
    C_FN = "doctor_enabled_cli_toolsets"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
