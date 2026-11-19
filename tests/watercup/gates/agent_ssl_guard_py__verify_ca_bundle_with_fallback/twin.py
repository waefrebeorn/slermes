"""twin for agent/ssl_guard.py:verify_ca_bundle_with_fallback (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/ssl_guard.py"
    PY_FN = "verify_ca_bundle_with_fallback"
    C_FN = "verify_ca_bundle_with_fallback"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
