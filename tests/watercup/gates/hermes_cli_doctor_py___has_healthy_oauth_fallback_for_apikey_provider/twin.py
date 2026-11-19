"""twin for hermes_cli/doctor.py:_has_healthy_oauth_fallback_for_apikey_provider (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/doctor.py"
    PY_FN = "_has_healthy_oauth_fallback_for_apikey_provider"
    C_FN = "doctor_has_healthy_oauth_fallback"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
