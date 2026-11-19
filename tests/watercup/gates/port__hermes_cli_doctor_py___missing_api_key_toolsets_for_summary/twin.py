"""twin for hermes_cli/doctor.py:_missing_api_key_toolsets_for_summary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase
import json as _j



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_missing_api_key_toolsets_for_summary"
    C_FN = "doctor_missing_api_key_toolsets"

    def norm(self, c, result):
        c.valstr(_j.dumps(result, sort_keys=True, separators=(",", ":")) if result is not None else "null")


_t = _Twin()
trace = _t.trace
run = _t.run
