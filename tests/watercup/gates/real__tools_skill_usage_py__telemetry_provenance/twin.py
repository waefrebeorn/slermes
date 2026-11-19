"""twin for tools/skill_usage.py:telemetry_provenance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/skill_usage.py"
    PY_FN = "telemetry_provenance"
    C_FN = "telemetry_provenance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
